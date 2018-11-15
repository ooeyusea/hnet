#include "Cache.h"
#include "util.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"

bool Cache::Start() {
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
