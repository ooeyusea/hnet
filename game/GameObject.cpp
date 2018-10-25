#include "GameObject.h"
#include <chrono>
#include "util.h"

void GameObject::Start() {
	auto co = DoWork([this] { CheckAlive(); });

	if (!ShakeHands()) {
		_terminate = false;
		co.Wait();
		return;
	}

	if (!Auth()) {
		_terminate = false;
		co.Wait();
		return;
	}

	DoWork([this] { DealPacket(); }).Wait();

	_terminate = true;
	co.Wait();

	Exit();
}

void GameObject::Cleanup() {

}

void GameObject::CheckAlive() {
	_aliveTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	while (!_terminate) {
		hn_sleep 1000;

		int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		if (now - _aliveTick >= 30000)
			break;
	}

	hn_close(_fd);
}

bool GameObject::ShakeHands() {


	return true;
}

bool GameObject::Auth() {

	return true;
}

void GameObject::DealPacket() {
	while (true) {
		
	}
}

void GameObject::Exit() {

}
