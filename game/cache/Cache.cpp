#include "Cache.h"
#include "AccountCache.h"
#include "RoleCache.h"

bool Cache::Start() {
	AccountCache::Instance().Start();
	RoleCache::Instance().Start();
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
