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

void RoleCache::Start() {
	//Cluster::Instance().Get().RegisterFn<256>(rpc_def::LOAD_ACCOUNT, [this](const std::string& userId, int16_t gate, int32_t fd) -> rpc_def::LoadAccountAck {
	//	rpc_def::LoadAccountAck ack;
	//	auto ptr = _accounts.FindCreate(userId);
	//	if (ptr) {
	//		AccountTable::UnitType::Locker lock(ptr);
	//		if (ptr->Get()) {
	//
	//		}
	//
	//	}
	//
	//	ack.errCode = err_def::LOAD_ACCOUNT_CREATE_FAILED;
	//	return ack;
	//});
	//
	//Cluster::Instance().Get().RegisterFn<256>(rpc_def::CREATE_ACTOR, [this](const std::string& userId, const std::string& name) {
	//
	//});
}
