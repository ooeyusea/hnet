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
#define SAVE_INTERVAL (360 * 1000)
#define RECOVER_TIMEOUT (24 * 3600 * 1000)
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
			return false;
		}

		if (!rs.Next()) {
			return false;
		}

		int16_t selfZone = ZONE_FROM_ID(Cluster::Instance().GetId());
		int16_t zone = rs.ToInt16(0);
		if (selfZone != zone) {
			try {
				int32_t cacheId = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(zone, ID_FROM_ID(Cluster::Instance().GetId())));
				bool kicked = Cluster::Instance().Get().Call(cacheId).Do<bool, 128>(rpc_def::KILL_ACTOR_CACHE, roleId, selfZone);
				if (!kicked) {
					return false;
				}

				rs.CloseResult();
				ret = Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(roleId, sql, rs);
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

		_name = rs.ToString(1);

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
				}
			}

			delete ticker;
		};
	}
}

bool Role::Landing(int64_t roleId) {
	char sql[MAX_LANDING_SQL_SIZE];
	int32_t len = SafeSprintf(sql, sizeof(sql), "update role set name = '");
	int32_t size = Holder<MysqlExecutor, db_def::GAME>::Instance()->EscapeString(_name.c_str(), _name.size(), sql + len);
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
				}
			}

			delete ticker;
		};
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
			rpc_def::TestData<rpc_def::RoleData, bool, true> kicked = Cluster::Instance().Get().Call(Cluster::Instance().ServiceId(node_def::LOGIC, logicIdx))
				.Do<rpc_def::TestData<rpc_def::RoleData, bool, true>, 128>(rpc_def::KILL_ACTOR, roleId, (int32_t)err_def::KICK_BY_ACCOUNT);

			if (!kicked.test) {
				return false;
			}

			Save(kicked.data);
		}
		catch (hn_rpc_exception& e) {
			return false;
		}
	}
	return true;
}

void RoleCache::Start() {
	Cluster::Instance().Get().Register(rpc_def::LOAD_ACTOR).AddCallback<MAX_ROLE_DATA>([this](int64_t roleId, int16_t logic) -> rpc_def::TestData<rpc_def::RoleData, int32_t, 0> {
		rpc_def::TestData<rpc_def::RoleData, int32_t, 0> ack;
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				if (!role->Load(roleId)) {
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

				return ack;
			}
		}
	
		ack.test = err_def::LOAD_ACCOUNT_CREATE_FAILED;
		return ack;
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();
	
	Cluster::Instance().Get().Register(rpc_def::SAVE_ACTOR).AddCallback<256>([this](int64_t roleId, const rpc_def::RoleData& data, bool remove) {
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				role->Save(data);
				role->StartLanding(roleId, SAVE_INTERVAL);
				if (remove) {
					role->SetLogic(false);
					role->StartRecover(roleId, RECOVER_TIMEOUT);
				}
			}
		}
	}).AddOrder([](int64_t roleId) -> int64_t {
		return roleId;
	}).Comit();

	Cluster::Instance().Get().Register(rpc_def::KILL_ACTOR_CACHE).AddCallback<256>([this](int64_t roleId, int16_t zone) {
		auto ptr = _roles.FindCreate(roleId);
		if (ptr) {
			RoleTable::UnitType::Locker lock(ptr);
			Role * role = ptr->Get();
			if (role) {
				if (!role->Kick(roleId))
					return false;

				if (!role->Landing(roleId))
					return false;
				role->StopLanding();

				if (!TransforTo(roleId, zone))
					return false;
				role->StopRecover();

				_roles.Remove(roleId, [&ptr]() {
					ptr->Release();
				});
			}
		}
		else {
			if (!TransforTo(roleId, zone))
				return false;
		}
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
					role->StartLanding(roleId, SAVE_INTERVAL);
				}
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
					role->StartRecover(roleId, RECOVER_TIMEOUT);
					return false;
				}
				role->StopLanding();

				_roles.Remove(roleId, [&ptr]() {
					ptr->Release();
				});
			}
			return true;
		}
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
