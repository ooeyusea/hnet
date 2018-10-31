#include "Session.h"
#include <chrono>
#include "util.h"

void Session::Start() {
	auto co = util::DoWork([this] { CheckAlive(); });

	if (!ShakeHands()) {
		_terminate = false;
		co.Wait();
		return;
	}

	//register session;

	util::DoWork([this] { DealPacket(); }).Wait();

	_terminate = true;
	co.Wait();

	//clean session;
	Exit();
}

void Session::CheckAlive() {
	_aliveTick = util::GetTickCount();
	while (!_terminate) {
		hn_sleep 1000;

		int64_t now = util::GetTickCount();
		if (now - _aliveTick >= 30000)
			break;
	}

	hn_close(_fd);
}

bool Session::ShakeHands() {


	return true;
}

void Session::DealPacket() {
	while (true) {
		
	}
}

void Session::Exit() {

}
