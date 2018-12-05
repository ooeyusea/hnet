#include "Session.h"
#include <chrono>
#include "util.h"
#include "websocket.h"
#include "rpc.h"
#include "nodedefine.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "clientdefine.h"
#include "errordefine.h"
#include "nodedefine.h"

#define LOGIN_GATE_MAX_CONNECT_TIME 12000
#define MAX_PACKET_SIZE 2048

void Session::Start() {
	auto co = util::DoWork([this] {
		hn_sleep LOGIN_GATE_MAX_CONNECT_TIME;
		_socket.Shutdown();
	});

	do {
		if (!_socket.ShakeHands())
			break;

		hn_trace("session [{}=>{}:{}] connected", _socket.GetFd(), _ip, _port);

		if (!Auth())
			break;

		if (_socket.IsConnected())
			break;

		if (!BindAccount())
			break;
	} while (false);

	_socket.Close();
	co.Wait();

	hn_trace("session [{}=>{}:{}] disconnect", _socket.GetFd(), _ip, _port);
}

bool Session::Auth() {
	client_def::AuthReq req;
	if (!_socket.Read(client_def::c2s::AUTH_REQ, 0, req)) {
		hn_debug("session[{}:{}] read auth req failed", _ip, _port);
		return false;
	}

	hn_trace("session[{}:{}] read auth req {}", _ip, _port, req.auth);

	_userId = std::move(req.auth);
	return true;
}

bool Session::BindAccount() {
	client_def::AuthRsp rsp;

	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);
	try {
		rpc_def::BindAccountAck ack = Cluster::Instance().Get().Call(accountIdx)
			.Do<rpc_def::BindAccountAck, 256, const std::string&>(rpc_def::BIND_ACCOUNT, _userId, ZONE_FROM_ID(id));
		if (ack.errCode != 0) {
			rsp.errCode = ack.errCode;

			_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);

			hn_trace("session[{}:{}] bind auth {} failed {}", _ip, _port, _userId, rsp.errCode);
			return false;
		}

		rsp.errCode = err_def::NONE;
		rsp.ip = ack.ip;
		rsp.check = ack.check;
		rsp.port = ack.port;

		_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);

		hn_info("session[{}:{}] bind auth {} success for check [{}:{}]:{}", _ip, _port, _userId, ack.ip, ack.port, rsp.check);
		return true;
	}
	catch (hn_rpc_exception& e) {
		rsp.errCode = err_def::AUTH_TIMEOUT;
		_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);

		hn_trace("session[{}:{}] bind auth {} timeout {}", _ip, _port, _userId, e.what());
		return false;
	}
}
