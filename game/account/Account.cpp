#include "Account.h"
#include "util.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"

#define MAX_KICK_COUNT 3
#define GATE_LOST_TIMEOUT 5000

bool Account::Start() {
	srand((int32_t)util::GetTimeStamp());

	Cluster::Instance().Get().Register(rpc_def::BIND_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t zone) -> rpc_def::BindAccountAck {
		rpc_def::BindAccountAck ack;
		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId > 0 && user->fd > 0 && user->kickCount > MAX_KICK_COUNT) {
					try {
						bool ok = Cluster::Instance().Get().Call(Cluster::Instance().ServiceId(node_def::GATE, user->gateId))
							.Do<bool, 128, int32_t, const std::string&>(rpc_def::KICK_USER, user->fd, userId, (int32_t)err_def::OTHER_USER_LOGIN);
						if (!ok) {
							++user->kickCount;
							ack.errCode = err_def::KICK_FAILED;
							return ack;
						}
					}
					catch (hn_rpc_exception& e) {
						++user->kickCount;
						ack.errCode = err_def::INVALID_USER;
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
				}
				else
					ack.errCode = err_def::NO_GATE;
			}
			else
				ack.errCode = err_def::INVALID_USER;
			return ack;
		}
		
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::LOGIN_ACCOUNT).AddCallback<128>([this](const std::string& userId, int16_t gate, int64_t check, int32_t fd) -> int32_t {
		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId == gate && user->check == check) {
					user->fd = fd;
					return err_def::NONE;
				}
				
				return err_def::INVALID_USER;
			}
		}

		return err_def::INVALID_USER;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::LOGIN_ACCOUNT).AddCallback<128>([this](const std::string& userId, int16_t gate, int32_t fd) {
		auto ptr = _users.FindCreate(userId);
		if (ptr) {
			UserTable::UnitType::Locker lock(ptr);
			User * user = ptr->Get();
			if (user) {
				if (user->gateId == gate && user->fd == fd) {
					user->fd = 0;
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
			if (gate.id != 0 && (now - gate.activeTime) < GATE_LOST_TIMEOUT) {
				if (ret == nullptr || ret->count > gate.count)
					ret = &gate;
			}
		}
	}
	return ret;
}
