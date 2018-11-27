#include "Logic.h"
#include "initializor.h"

bool Logic::Start() {
	InitializorMgr::Instance().Start();
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
