#include "Gate.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "websocket.h"
#include "clientdefine.h"

#define DELAY_OPEN_INTERVAL 15000

bool Gate::Start() {
	Cluster::Instance().Get().RegisterFn<128>(rpc_def::KICK_USER, [this](int32_t fd, const std::string& userId, int32_t reason) {
		bool kick = false;
		int32_t version = 0;
		{
			std::lock_guard<spin_mutex> lock(_mutex);
			auto itr = _sessions.find(fd);
			if ((itr != _sessions.end() && itr->second->GetUserId() == userId)) {
				kick = true;
				version = itr->second->GetVersion();
			}
		}

		if (kick) {
			client_def::KickNtf ntf;
			ntf.errCode = reason;

			websocket::WebSocketSender ws(fd);
			ws.Write<1024>(client_def::s2c::KICK_NTF, version, ntf);

			hn_close(fd);
		}
		return true;
	});

	hn_sleep DELAY_OPEN_INTERVAL;

	_listenFd = hn_listen("127.0.0.1", 9025);
	if (_listenFd < 0)
		return false;

	return true;
}

void Gate::Run() {
	auto co = util::DoWork([this] {
		while (true) {
			char ipStr[64] = { 0 };
			int32_t port = 0;
			int32_t fd = hn_accept(_listenFd, ipStr, sizeof(ipStr) - 1, &port);
			if (fd < 0)
				break;

			std::string ip = ipStr;
			hn_fork [this, fd, ip, port]() {
				Session object(fd, ip, port);
				RegisterSession(&object);
				object.Start();
				UnregisterSession(&object);
			};
		}
	});

	int8_t res;
	_closeCh >> res;

	hn_close(_listenFd);
	co.Wait();
}

void Gate::Release() {

}

void Gate::Terminate() {
	_closeCh.TryPush(1);
}
