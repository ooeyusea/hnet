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

		if (!Auth())
			break;

		if (_socket.IsConnected())
			return;

		if (!BindAccount())
			break;
	} while (false);

	_socket.Close();
	co.Wait();
}

bool Session::Auth() {
	int32_t size = 0;
	const char * data = _socket.ReadFrame(size);
	if (data == nullptr)
		return false;

	if (*(int32_t*)data != client_def::c2s::AUTH_REQ)
		return false;

	hn_istream stream(data, size);
	hn_iachiver ar(stream, 0);
	
	client_def::AuthReq req;
	ar >> req;

	if (ar.Fail())
		return false;

	_userId = std::move(req.auth);
	return true;
}

bool Session::BindAccount() {
	client_def::AuthRsp rsp;

	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);
	try {
		rpc_def::BindAccountAck ack = Cluster::Instance().Get().Call<rpc_def::BindAccountAck, 256, int16_t, const std::string&>(accountIdx, hn_rpc_order{util::CalcUniqueId(_userId.c_str())}, rpc_def::BIND_ACCOUNT, ZONE_FROM_ID(id), _userId);
		if (ack.errCode != 0) {
			rsp.errCode = ack.errCode;

			_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);
			return false;
		}

		rsp.errCode = err_def::NONE;
		rsp.ip = ack.ip;
		rsp.check = ack.check;
		rsp.port = ack.port;

		_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);
		return true;
	}
	catch (hn_rpc_exception& e) {
		rsp.errCode = err_def::AUTH_TIMEOUT;
		_socket.Write<128>(client_def::s2c::AUTH_RSP, 0, rsp);
		return false;
	}
}
