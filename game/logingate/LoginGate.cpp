#include "LoginGate.h"
#include "Session.h"
#include "util.h"
#include "servernode.h"

bool LoginGate::Start() {
	_listenFd = hn_listen("127.0.0.1", 9025);
	if (_listenFd < 0)
		return false;

	return true;
}

void LoginGate::Run() {
	auto co = util::DoWork([this] {
		while (true) {
			char ipStr[64] = { 0 };
			int32_t port = 0;
			int32_t fd = hn_accept(_listenFd, ipStr, sizeof(ipStr) - 1, &port);
			if (fd < 0)
				break;

			std::string ip = ipStr;
			hn_fork [fd, ip, port]() {
				Session object(fd, ip, port);
				object.Start();
			};
		}
	});

	int8_t res;
	_closeCh >> res;

	hn_close(_listenFd);
	co.Wait();
}

void LoginGate::Release() {

}

void LoginGate::Terminate() {
	_closeCh.TryPush(1);
}
