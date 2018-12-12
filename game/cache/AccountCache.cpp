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
#include "id/id.h"

bool Account::Load(const std::string& userId) {
	if (!_loadData) {
		char sql[1024];
		SafeSprintf(sql, sizeof(sql), "select zone from account where userId = '%s'", userId.c_str());
		int64_t uniqueId = util::CalcUniqueId(userId.c_str());

		MysqlExecutor::ResultSet rs;
		int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql, rs);
		if (ret < 0) {
			hn_error("account {} load data failed", userId);
			return false;
		}

		int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());
		if (rs.Next()) {
			int16_t zone = rs.ToInt16(0);
			if (selfZone != zone) {
				hn_info("account {} zone is not match {} / {}", userId, selfZone, zone);

				try {
					int32_t cacheId = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(zone, ID_FROM_ID(Cluster::Instance().GetId())));
					bool kicked = Cluster::Instance().Get().Call(cacheId).Do<bool, 128, const std::string&>(rpc_def::KILL_ACCOUNT_CACHE, userId, selfZone);
					if (!kicked) {
						hn_error("account {} kick from zone {} failed", userId, zone);
						return false;
					}

					rs.CloseResult();
					ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql, rs);
					if (ret < 0) {
						hn_error("account {} reload data failed", userId);
						return false;
					}

					if (!rs.Next()) {
						hn_error("account {} reload data failed", userId);
						return false;
					}

					hn_info("account {} kick from zone {} success", userId, zone);
				}
				catch (hn_rpc_exception & e) {
					hn_error("account {} kick from zone {} rpc failed {}", userId, zone, e.what());
					return false;
				}
			}
		}
		else {
			hn_info("account {} is not register, now register {}", userId, selfZone);

			SafeSprintf(sql, sizeof(sql), "insert into account(userId, zone) values ('%s', %d)", userId.c_str(), selfZone);

			ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql);
			if (ret < 0) {
				hn_info("account {} is not register, now register {}", userId, selfZone);
				return false;
			}
		}

		rs.CloseResult();
		SafeSprintf(sql, sizeof(sql), "select id, name from role where userId = '%s'", userId.c_str());
		ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql);
		if (ret < 0) {
			hn_error("account {} load role failed", userId);
			return false;
		}

		if (rs.Next()) {
			_hasRole = true;

			_role.id = rs.ToInt64(0);
			_role.name = rs.ToString(1);

			hn_info("account {} load role {} {}", userId, _role.id, _role.name);
		}
		else {
			_hasRole = false;

			hn_info("account {} load role no role", userId);
		}

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
				hn_error("account {} kick fd {}:{} failed", userId, _gate, _fd);
				return false;
			}

			hn_info("account {} kick fd {}:{} succcess", userId, _gate, _fd);
		}
		catch (hn_rpc_exception& e) {
			hn_error("account {} kick fd {}:{} rpc failed {}", userId, _gate, _fd, e.what());
			return false;
		}
	}
	return true;
}

int64_t Account::CreateRole(const std::string& userId, const rpc_def::RoleCreater& creator) {
	int64_t id = IdGeter::Instance().Get();
	if (id == 0)
		return 0;

	int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());
	int64_t uniqueId = util::CalcUniqueId(userId.c_str());

	char sql[1024];
	SafeSprintf(sql, sizeof(sql), "insert into `role`(`id`, `userId`, `name`, `zone`) values (%lld, '%s', '%s', %d)", id, userId.c_str(), creator.name.c_str(), selfZone);
	int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(uniqueId, sql);
	if (ret < 0)
		return 0;

	_hasRole = true;
	_role.id = id;
	_role.name = creator.name;

	return id;
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
					hn_error("account recover rpc failed {}", e.what());

				}
			}

			delete ticker;
		};

		hn_info("account {} start recover", userId);
	}
}

void AccountCache::Start(int32_t accountTimeout) {
	Cluster::Instance().Get().Register(rpc_def::LOAD_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t gate, int32_t fd) -> rpc_def::LoadAccountAck {
		hn_info("account {} load data {} {}", userId, gate, fd);

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

					hn_info("account {} load data failed", userId);
					return ack;
				}

				if (!account->Kick(userId)) {
					ack.errCode = err_def::KICK_OTHERLOGIN_FAILED;
					hn_info("account {} kick failed", userId);
					return ack;
				}

				account->StopRecover();
				account->Set(gate, fd);

				ack.errCode = err_def::NONE;
				account->Pack(ack);

				hn_info("account {} load success", userId);
				return ack;
			}
		}

		ack.errCode = err_def::LOAD_ACCOUNT_CREATE_FAILED;
		return ack;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::RECOVER_ACCOUNT).AddCallback<256>([this](const std::string& userId, int16_t gate, int32_t fd) {
		hn_info("account {} recover", userId, gate, fd);

		auto ptr = _accounts.Find(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (account->GetGate() == gate && account->GetFd() == fd)
					account->StartRecover(userId, _accountTimeout);
			}
		}
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::CREATE_ACTOR).AddCallback<256>([this](const std::string& userId, const rpc_def::RoleCreater& creator) {
		hn_info("account {} create role {}", userId, creator.name);

		rpc_def::CreateRoleAck ack;
		auto ptr = _accounts.Find(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				//check name

				if (account->HasRole()) {
					ack.errCode = err_def::ROLE_FULL;
					hn_info("account {} create role {} but already has role", userId, creator.name);
					return ack;
				}

				ack.roleId = account->CreateRole(userId, creator);
				ack.errCode = (ack.roleId != 0 ? err_def::NONE : err_def::CREATE_ROLE_FAILED);

				hn_info("account {} create role {} success {} ", userId, creator.name, ack.roleId);
				return ack;
			}
		}

		ack.errCode = err_def::LOAD_ACCOUNT_FAILED;
		return ack;
	}).AddOrder([](const std::string& userId)->int64_t {
		return util::CalcUniqueId(userId.c_str());
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::KILL_ACCOUNT_CACHE).AddCallback<256>([this](const std::string& userId, int16_t zone) {
		hn_info("account {} kill cache from zone {}", userId, zone);

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

					hn_info("account {} transfor to zone {}", userId, zone);
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

				hn_info("account {} transfor to zone {}", userId, zone);
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

