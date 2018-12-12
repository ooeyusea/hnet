#include "RoleCache.h"
#include "util.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "object_holder.h"
#include "dbdefine.h"
#include "mysqlmgr.h"

#define MAX_ROLE_DATA 4096
#define MAX_LANDING_SQL_SIZE 65536

Role::Role() {

}

Role::~Role() {

}

bool Role::Load(int64_t roleId) {
	if (!_loadData) {
		char sql[1024];
		SafeSprintf(sql, sizeof(sql), "select zone, name from role where roleId = %lld", roleId);

		MysqlExecutor::ResultSet rs;
		int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(roleId, sql, rs);
		if (ret < 0) {
			hn_error("load role {} data from db failed", roleId);
			return false;
		}

		if (!rs.Next()) {
			hn_error("load role {} data from db has no role", roleId);
			return false;
		}

		int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());
		int16_t zone = rs.ToInt16(0);
		if (selfZone != zone) {
			hn_info("role {} zone is not match {}/{}", roleId, selfZone, zone);

			try {
				int32_t cacheId = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(zone, ID_FROM_ID(Cluster::Instance().GetId())));
				bool kicked = Cluster::Instance().Get().Call(cacheId).Do<bool, 128>(rpc_def::KILL_ACTOR_CACHE, roleId, selfZone);
				if (!kicked) {
					hn_error("role {} kill from zone {} failed", roleId, zone);
					return false;
				}

				rs.CloseResult();
				ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(roleId, sql, rs);
				if (ret < 0) {
					hn_error("role {} reload from db failed", roleId);
					return false;
				}

				if (!rs.Next()) {
					hn_error("role {} reload from db failed", roleId);
					return false;
				}
			}
			catch (hn_rpc_exception & e) {
				hn_error("role {} kill from zone {} rpc failed {}", roleId, zone, e.what());
				return false;
			}
		}

		_name = rs.ToString(1);

		hn_info("role {} load data success", roleId);
		_loadData = true;
	}

	return true;
}

void Role::StartLanding(int64_t roleId, int64_t timeout) {
	if (!_landingTicker) {
		_landingTicker = new hn_ticker(timeout, 1);

		auto ticker = _landingTicker;
		hn_fork[roleId, ticker, this]() {
			for (auto i : *ticker) {
				try {
					Cluster::Instance().Get().Call(hyper_net::JUST_CALL).Do<128, int64_t>(rpc_def::LANDING_ACTOR, roleId);
				}
				catch (hn_rpc_exception & e) {
					hn_error("role landing rpc failed {}", e.what());
				}
			}

			delete ticker;
		};

		hn_info("role {} start landing", roleId);
	}
}

bool Role::Landing(int64_t roleId) {
	char sql[MAX_LANDING_SQL_SIZE];
	int32_t len = SafeSprintf(sql, sizeof(sql), "update role set name = '");
	int32_t size = Holder<MysqlExecutor, db_def::GAME>::Instance()->EscapeString(_name.c_str(), (int32_t)_name.size(), sql + len);
	len += size;
	SafeSprintf(sql + len, sizeof(sql) - len, "' where roleId = %lld", roleId);
	
	int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(roleId, sql);
	return ret > 0;
}

void Role::StartRecover(int64_t roleId, int64_t timeout) {
	if (!_recoverTicker) {
		_recoverTicker = new hn_ticker(timeout, 1);

		auto ticker = _recoverTicker;
		hn_fork[roleId, ticker]() {
			for (auto i : *ticker) {
				try {
					Cluster::Instance().Get().Call(hyper_net::JUST_CALL).Do<128, int64_t>(rpc_def::REMOVE_ACTOR_CACHE, roleId, rpc_def::JustCallPtr<hn_ticker>{ticker});
				}
				catch (hn_rpc_exception & e) {
					hn_error("role recover rpc failed {}", e.what());
				}
			}

			delete ticker;
		};

		hn_info("role {} start recover", roleId);
	}
}

void Role::Pack(rpc_def::RoleData & data) {
	data.name = _name;
}

void Role::Save(const rpc_def::RoleData & data) {
	_name = data.name;
}

bool Role::Kick(int64_t roleId) {
	if (_inLogic) {
		int16_t id = Cluster::Instance().GetId();
		int16_t logic = Zone::Instance().Calc(node_def::LOGIC, ZONE_FROM_ID(id), roleId);
		int16_t logicIdx = Cluster::Instance().ServiceId(node_def::LOGIC, ID_FROM_ZONE(ZONE_FROM_ID(id), logic));

		try {
			rpc_def::KickPlayerAck kicked = Cluster::Instance().Get().Call(Cluster::Instance().ServiceId(node_def::LOGIC, logicIdx))
				.Do<rpc_def::KickPlayerAck, 128>(rpc_def::KILL_ACTOR, roleId, (int32_t)err_def::KICK_BY_ACCOUNT);

			if (!kicked.test) {
				hn_error("role {} kick from logic failed", roleId);
				return false;
			}

			if (kicked.data.test)
				Save(kicked.data.data);

			hn_info("role {} kick from logic", roleId);
		}
		catch (hn_rpc_exception& e) {
			hn_error("role {} kick from logic rpc failed {}", roleId, e.what());
			return false;
		}
	}
	return true;
}

void RoleCache::Start(int32_t saveInterval, int32_t recoverTimeout) {
	_saveInterval = saveInterval;
	_recoverTimeout = recoverTimeout;

	Cluster::Instance().Get().Register(rpc_def::LOAD_ACTOR).AddCallback<MAX_ROLE_DATA>([this](int64_t roleId, int16_t logic) -> rpc_def::TestData<rpc_def::RoleData, int32_t, 0> {
		hn_info("role {} load from logic", roleId);

		rpc_def::TestData<rpc_def::RoleData, int32_t, 0> ack;
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				if (!role->Load(roleId)) {
					hn_error("role {} load data failed", roleId);

					ack.test = err_def::LOAD_ROLE_FAILED;

					_roles.Remove(roleId, [&ptr]() {
						ptr->Release();
					});
					return ack;
				}
				role->SetLogic(true);

				ack.test = err_def::NONE;

				role->StopRecover();
				role->Pack(ack.data);

				hn_info("role {} load data complete", roleId);
				return ack;
			}
		}
	
		ack.test = err_def::LOAD_ACCOUNT_CREATE_FAILED;
		return ack;
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();
	
	Cluster::Instance().Get().Register(rpc_def::SAVE_ACTOR).AddCallback<256>([this](int64_t roleId, const rpc_def::RoleData& data, bool remove) {
		hn_trace("role {} save from logic", roleId);
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				role->Save(data);
				role->StartLanding(roleId, _saveInterval);
				if (remove) {
					role->SetLogic(false);
					role->StartRecover(roleId, _recoverTimeout);
				}
			}
		}
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::KILL_ACTOR_CACHE).AddCallback<256>([this](int64_t roleId, int16_t zone) {
		hn_info("role {} kill cache from zone {}", roleId, zone);

		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				if (!role->Kick(roleId)) {
					hn_error("role {} kick from logic failed stop clear cache", roleId);
					return false;
				}

				if (!role->Landing(roleId)) {
					hn_error("role {} landing failed stop clear cache", roleId);
					return false;
				}
				role->StopLanding();

				if (!TransforTo(roleId, zone))
					return false;

				hn_info("role {} transfor to zone {}", roleId, zone);
				role->StopRecover();

				_roles.Remove(roleId, [&ptr]() {
					ptr->Release();
				});
			}
		}
		else {
			if (!TransforTo(roleId, zone))
				return false;

			hn_info("role {} transfor to zone {}", roleId, zone);
		}
		return true;
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::LANDING_ACTOR).AddCallback<256>([this](int64_t roleId) {
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				if (!role->Landing(roleId)) {
					role->StopLanding();
					role->StartLanding(roleId, _saveInterval);

					hn_error("role {} landing failed delay landing", roleId);
				}

				hn_trace("role {} landing success", roleId);
			}
		}
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::REMOVE_ACTOR_CACHE).AddCallback<256>([this](int64_t roleId, rpc_def::JustCallPtr<hn_ticker> ticker) {
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role && role->CheckRecover(ticker.ptr)) {
				if (!role->Landing(roleId)) {
					role->StopRecover();
					role->StartRecover(roleId, _recoverTimeout);

					hn_error("role {} landing failed delay remove", roleId);
					return false;
				}
				role->StopLanding();
				hn_info("role {} removed", roleId);

				_roles.Remove(roleId, [&ptr]() {
					ptr->Release();
				});
			}
			return true;
		}
		return true;
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();
}

bool RoleCache::TransforTo(int64_t roleId, int16_t zone) {
	int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());

	char sql[1024];
	snprintf(sql, 1023, "update role set zone = %d where id = %lld and zone = %d", zone, roleId, selfZone);

	int32_t ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(roleId, sql);
	return ret > 0;
}
