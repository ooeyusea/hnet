#include "LoginGate.h"
#include "Session.h"
#include "util.h"
#include "servernode.h"
#include "XmlReader.h"
#include "nodedefine.h"
#include "argument.h"

LoginGate::LoginGate() {
	Argument::Instance().RegArgument("node", 0, _loginGateId);
}

bool LoginGate::Start() {
	if (!ReadConf())
		return false;

	_listenFd = hn_listen("0.0.0.0", _listenPort);
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

bool LoginGate::ReadConf() {
	olib::XmlReader conf;
	if (!conf.LoadXml("conf.xml")) {
		return false;
	}

	try {
		auto& servers = conf.Root()["server"];
		for (int32_t i = 0; i < servers.Count(); ++i) {
			int8_t type = servers[i].GetAttributeInt8("type");
			int16_t id = servers[i].GetAttributeInt16("id");

			if (type == node_def::LOGINGATE && id == _loginGateId) {
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
