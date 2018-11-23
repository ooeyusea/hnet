#include "Logic.h"
#include "RoleMgr.h"

bool Logic::Start() {
	RoleMgr::Instance().Start();
	return true;
}

void Logic::Run() {
	int8_t res;
	_closeCh >> res;
}

void Logic::Release() {

}

void Logic::Terminate() {
	_closeCh.TryPush(1);
}
