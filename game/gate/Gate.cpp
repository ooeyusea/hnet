#include "Gate.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "websocket.h"
#include "clientdefine.h"
#include "argument.h"
#include "XmlReader.h"
#include "nodedefine.h"

#define DELAY_OPEN_INTERVAL 15000

Gate::Gate() {
	Argument::Instance().RegArgument("node", 0, _gateId);
}

bool Gate::Start() {
	if (!ReadConf())
		return false;

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

	_listenFd = hn_listen("0.0.0.0", _listenPort);
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

bool Gate::ReadConf() {
	olib::XmlReader conf;
	if (!conf.LoadXml("conf.xml")) {
		return false;
	}

	try {
		auto& servers = conf.Root()["server"];
		for (int32_t i = 0; i < servers.Count(); ++i) {
			int8_t type = servers[i].GetAttributeInt8("type");
			int16_t id = servers[i].GetAttributeInt16("id");

			if (type == node_def::GATE && id == _gateId) {
				_listenPort = servers[i].GetAttributeInt32("port");
			}
		}
	}
	catch (std::exception& e) {
		return false;
	}

	if (_listenPort == 0)
		return false;

	return true;
}
