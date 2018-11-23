#include "AccountCache.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "mysqlmgr.h"
#include "dbdefine.h"
#include "object_holder.h"
#define RECOVER_TIMEOUT (360 * 1000)

bool Account::Load(const std::string& userId) {
	if (!_loadData) {
		char sql[1024];
		SafeSprintf(sql, sizeof(sql), "select zone from account where userId = '%s'", userId.c_str());
		int64_t uniqueId = util::CalcUniqueId(userId.c_str());

		MysqlExecutor::ResultSet rs;
		int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql, rs);
		if (ret < 0)
			return false;

		int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());
		if (rs.Next()) {
			int16_t zone = rs.ToInt16(0);
			if (selfZone != zone) {
				try {
					int32_t cacheId = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(zone, ID_FROM_ID(Cluster::Instance().GetId())));
					bool kicked = Cluster::Instance().Get().Call(cacheId).Do<bool, 128, const std::string&>(rpc_def::KILL_ACCOUNT_CACHE, userId, selfZone);
					if (!kicked) {
						return false;
					}

					rs.CloseResult();
					ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql, rs);
					if (ret < 0) {
						return false;
					}

					if (!rs.Next()) {
						return false;
					}
				}
				catch (hn_rpc_exception & e) {
					return false;
				}
			}
		}
		else {
			SafeSprintf(sql, sizeof(sql), "insert into account(userId, zone) values ('%s', %d)", userId.c_str(), selfZone);

			ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql);
			if (ret < 0)
				return false;
		}

		rs.CloseResult();
		SafeSprintf(sql, sizeof(sql), "select id, name from role where userId = '%s'", userId.c_str());
		ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql);
		if (ret < 0)
			return false;

		if (rs.Next()) {
			_hasRole = true;

			_role.id = rs.ToInt64(0);
			_role.name = rs.ToString(1);
		}
		else
			_hasRole = false;

		_loadData = true;
	}
	return true;
}

bool Account::Kick(const std::string& userId) {
	if (_fd > 0) {
		try {
			bool kicked = Cluster::Instance().Get().Call(Cluster::Instance().ServiceId(node_def::GATE, _gate))
				.Do<bool, 128, int32_t, const std::string&>(rpc_def::KICK_USER, _fd, userId, (int32_t)err_def::KICK_BY_ACCOUNT);

			if (!kicked) {
				return false;
			}
		}
		catch (hn_rpc_exception& e) {
			return false;
		}
	}
	return true;
}

int64_t Account::CreateRole(const rpc_def::RoleCreater& creator) {
	return 0;
}

void Account::Pack(rpc_def::LoadAccountAck& ack) {
	ack.hasRole = _hasRole;
	if (_hasRole)
		ack.role = _role;
}

void Account::StartRecover(std::string userId, int64_t elapse) {
	if (!_ticker) {
		_ticker = new hn_ticker(elapse, 1);

		auto ticker = _ticker;
		hn_fork [userId, ticker, this]() {
			for (auto i : *ticker) {
				try {
					Cluster::Instance().Get().Call(hyper_net::JUST_CALL).Do<bool, 128, const std::string&>(rpc_def::KILL_ACCOUNT_CACHE, userId, 0);
				}
				catch (hn_rpc_exception & e) {
				}
			}

			delete ticker;
		};
	}
}

void AccountCache::Start() {
	Cluster::Instance().Get().Register(rpc_def::LOAD_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t gate, int32_t fd) -> rpc_def::LoadAccountAck {
		rpc_def::LoadAccountAck ack;
		auto ptr = _accounts.FindCreate(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (!account->Load(userId)) {
					ack.errCode = err_def::LOAD_ACCOUNT_FAILED;

					_accounts.Remove(userId, [&ptr]() {
						ptr->Release();
					});
					return ack;
				}

				if (!account->Kick(userId)) {
					ack.errCode = err_def::KICK_OTHERLOGIN_FAILED;
					return ack;
				}

				account->StopRecover();
				account->Set(gate, fd);

				ack.errCode = err_def::NONE;
				account->Pack(ack);
				return ack;
			}
		}

		ack.errCode = err_def::LOAD_ACCOUNT_CREATE_FAILED;
		return ack;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::RECOVER_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t gate, int32_t fd) {
		auto ptr = _accounts.Find(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (account->GetGate() == gate && account->GetFd() == fd)
					account->StartRecover(userId, RECOVER_TIMEOUT);
			}
		}
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::CREATE_ACTOR).AddCallback<256>([this](const std::string& userId, const rpc_def::RoleCreater& creator) {
		rpc_def::CreateRoleAck ack;
		auto ptr = _accounts.Find(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				//check name

				if (account->HasRole()) {
					ack.errCode = err_def::ROLE_FULL;
					return ack;
				}

				ack.roleId = account->CreateRole(creator);
				ack.errCode = (ack.roleId != 0 ? err_def::NONE : err_def::CREATE_ROLE_FAILED);

				return ack;
			}
		}

		ack.errCode = err_def::LOAD_ACCOUNT_FAILED;
		return ack;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::KILL_ACCOUNT_CACHE).AddCallback<256>([this](const std::string& userId, int16_t zone) {
		auto ptr = _accounts.Find(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (!account->Kick(userId))
					return false;

				if (zone != 0) {
					if (!TransforTo(userId, zone))
						return false;
				}
				account->StopRecover();

				_accounts.Remove(userId, [&ptr]() {
					ptr->Release();
				});
			}
		}
		else {
			if (zone != 0) {
				if (!TransforTo(userId, zone))
					return false;
			}
		}

		return true;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();
}

bool AccountCache::TransforTo( const std::string& userId, int16_t zone) {
	int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());

	char sql[1024];
	snprintf(sql, 1023, "update account set zone = %d where userId = '%s' and zone = %d", zone, userId.c_str(), selfZone);

	int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(util::CalcUniqueId(userId.c_str()), sql);
	return ret > 0;
}

