#include "Cache.h"
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

bool Cache::Start() {
	Cluster::Instance().Get().RegisterFn<256>(rpc_def::LOAD_ACCOUNT, [this](const std::string& userId) {

	});

	Cluster::Instance().Get().RegisterFn<256>(rpc_def::CREATE_ACTOR, [this](const std::string& userId, const std::string& name) {

	});

	return true;
}

void Cache::Run() {
	int8_t res;
	_closeCh >> res;
}

void Cache::Release() {

}

void Cache::Terminate() {
	_closeCh.TryPush(1);
}
