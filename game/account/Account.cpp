#include "Account.h"
#include "util.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"
#include "XmlReader.h"
#include "argument.h"

#define MAX_KICK_COUNT 3
#define GATE_LOST_TIMEOUT 5000

bool Account::Start() {
	srand((int32_t)util::GetTimeStamp());

	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

		if (conf.Root().IsExist("account") && conf.Root()["account"][0].IsExist("define")) {
			_maxKickCount = conf.Root()["account"][0]["define"][0].GetAttributeInt32("max_kick");
			_gateLostTimeout = conf.Root()["account"][0]["define"][0].GetAttributeInt32("timeout");
		}
		else {
			_maxKickCount = MAX_KICK_COUNT;
			_gateLostTimeout = GATE_LOST_TIMEOUT;
		}
	}
	catch (std::exception& e) {
		hn_error("Load Account Config : {}", e.what());
		return false;
	}

	Cluster::Instance().Get().Register(rpc_def::BIND_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t zone) -> rpc_def::BindAccountAck {
		hn_info("user {} bind from {}", userId, zone);

		rpc_def::BindAccountAck ack;
		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId > 0 && user->fd > 0 && user->kickCount > _maxKickCount) {
					try {
						bool ok = Cluster::Instance().Get().Call(Cluster::Instance().ServiceId(node_def::GATE, user->gateId))
							.Do<bool, 128, int32_t, const std::string&>(rpc_def::KICK_USER, user->fd, userId, (int32_t)err_def::OTHER_USER_LOGIN);
						if (!ok) {
							++user->kickCount;
							ack.errCode = err_def::KICK_FAILED;

							hn_error("user {} kick from gate {} failed", userId, user->gateId);
							return ack;
						}

						hn_info("user {} kick from gate {} success", userId, user->gateId);
					}
					catch (hn_rpc_exception& e) {
						++user->kickCount;
						ack.errCode = err_def::INVALID_USER;

						hn_error("user {} kick from gate {} rpc failed {}", userId, user->gateId, e.what());
						return ack;
					}
				}

				auto * gate = Choose(zone);
				if (gate) {
					user->gateId = gate->id;
					user->kickCount = 0;
					user->fd = 0;
					user->check = (util::GetTimeStamp() | rand());

					ack.errCode = err_def::NONE;
					ack.ip = gate->ip;
					ack.port = gate->port;
					ack.check = user->check;

					hn_info("user {} from {} distribute gate [{}:{}] check {}", userId, user->gateId, gate->ip, gate->port, user->check);
				}
				else
					ack.errCode = err_def::NO_GATE;
			}
			else
				ack.errCode = err_def::INVALID_USER;

			hn_info("user {} bind from {} result {}", userId, zone, ack.errCode);
			return ack;
		}

		ack.errCode = err_def::INVALID_USER;
		return ack;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::LOGIN_ACCOUNT).AddCallback<128>([this](const std::string& userId, int16_t gate, int64_t check, int32_t fd) -> int32_t {
		hn_trace("recv user {} login from gate {}:{} check {}", userId, gate, fd, check);

		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId == gate && user->check == check) {
					user->fd = fd;

					hn_info("user {} login from gate {}:{} check success {}", userId, gate, fd, check);
					return err_def::NONE;
				}
				
				hn_info("user {} login from gate {}:{}/{} check failed {}/{}", userId, gate, fd, user->gateId, check, user->check);
				return err_def::INVALID_USER;
			}
		}

		return err_def::INVALID_USER;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::LOGOUT_ACCOUNT).AddCallback<128>([this](const std::string& userId, int16_t gate, int32_t fd) {
		hn_trace("recv user {} logout from gate {}:{}", userId, gate, fd);

		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId == gate && user->fd == fd) {
					user->fd = 0;

					hn_info("user {} logout from gate {}:{}", userId, gate, fd);
				}
				else {
					hn_info("user {} logout from gate {}:{} check do nothing", userId, gate, fd);
				}
			}
		}
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::GATE_REPORT).AddCallback<128>([this](int16_t id, const std::string& ip, int32_t port, int32_t count) {
		auto& gate = _gates[ZONE_FROM_ID(id)][ID_FROM_ID(id)];
		gate.activeTime = util::GetTickCount();
		gate.ip = ip;
		gate.port = port;
		gate.count = count;

		hn_trace("gate {} report {}:{} {}", id, ip, port, count);
	}).Comit();

	return true;
}

void Account::Run() {
	int8_t res;
	_closeCh >> res;
}

void Account::Release() {

}

void Account::Terminate() {
	_closeCh.TryPush(1);
}

Account::Gate * Account::Choose(int16_t zone) {
	Gate * ret = nullptr;

	int64_t now = util::GetTickCount();
	if (zone > 0 && zone <= MAX_ZONE) {	
		int32_t start = rand() % MAX_ZONE_ID;
		for (int32_t i = 0; i < MAX_ZONE_ID; ++i) {
			Gate& gate = _gates[zone - 1][(i + start) % MAX_ZONE_ID];
			if (gate.id != 0 && (now - gate.activeTime) < _gateLostTimeout) {
				if (ret == nullptr || ret->count > gate.count)
					ret = &gate;
			}
		}
	}
	return ret;
}
