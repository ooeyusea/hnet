#include "AccountCache.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"
#include "servernode.h"
#include "rpcdefine.h"

bool Account::Load() {
	return true;
}

bool Account::Kick() {
	return true;
}

bool Account::TransforTo(int16_t zone) {
	return true;
}

void Account::Pack(rpc_def::LoadAccountAck& ack) {

}

int64_t Account::CreateRole(const rpc_def::RoleCreater& creator) {
	return 0;
}

void AccountCache::Start() {
	Cluster::Instance().Get().RegisterFn<256>(rpc_def::LOAD_ACCOUNT, [this](const std::string& userId, int16_t gate, int32_t fd) -> rpc_def::LoadAccountAck {
		rpc_def::LoadAccountAck ack;
		auto ptr = _accounts.FindCreate(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (!account->Kick()) {
					ack.errCode = err_def::KICK_OTHERLOGIN_FAILED;
					return ack;
				}
				if (!account->Load()) {
					ack.errCode = err_def::LOAD_ACCOUNT_FAILED;

					_accounts.Remove(userId, [&ptr]() {
						ptr->Release();
					});
					return ack;
				}

				ack.errCode = err_def::NONE;
				account->Pack(ack);
				return ack;
			}
		}

		ack.errCode = err_def::LOAD_ACCOUNT_CREATE_FAILED;
		return ack;
	});

	Cluster::Instance().Get().RegisterFn<256>(rpc_def::CREATE_ACTOR, [this](const std::string& userId, const rpc_def::RoleCreater& creator) {
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
	});

	Cluster::Instance().Get().RegisterFn<256>(rpc_def::KILL_ACCOUNT_CACHE, [this](const std::string& userId, int16_t zone) {
		auto ptr = _accounts.FindCreate(userId);
		if (ptr) {
			AccountTable::UnitType::Locker lock(ptr);
			Account * account = ptr->Get();
			if (account) {
				if (!account->Kick())
					return false;

				if (!account->TransforTo(zone))
					return false;

				_accounts.Remove(userId, [&ptr]() {
					ptr->Release();
				});
			}
			return true;
		}
		return true;
	});
}
