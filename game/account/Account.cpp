#include "Account.h"
#include "util.h"

bool Account::Start() {
	return true;
}

void Account::Run() {
	int8_t res;
	_closeCh >> res;
}

void Account::Release() {

}

void Account::Terminate() {
	_closeCh.TryPush(1);
}
