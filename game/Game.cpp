#include "Game.h"
#include "GameObject.h"
#include "util.h"

bool Game::Start() {
	_listenFd = hn_listen("127.0.0.1", 9025);
	if (_listenFd < 0)
		return false;

	return true;
}

void Game::Run() {
	auto co = DoWork([this] {
		while (true) {
			char ipStr[64] = { 0 };
			int32_t port = 0;
			int32_t fd = hn_accept_addr(_listenFd, ipStr, sizeof(ipStr) - 1, &port);
			if (fd < 0)
				break;

			std::string ip = ipStr;
			hn_fork [fd, ip, port]() {
				GameObject object(fd, ip, port);
				object.Start();
				object.Cleanup();
			};
		}
	});

	int8_t res;
	_closeCh >> res;

	hn_close(_listenFd);
	co.Wait();
}

void Game::Release() {

}

void Game::Terminate() {
	_closeCh.TryPush(1);
}
