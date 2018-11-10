#include "Session.h"
#include <chrono>
#include "util.h"
#include "websocket.h"
#include "rpc.h"
#include "nodedefine.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "clientdefine.h"

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

	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);
	rpc_def::BindAccountAck ack = Cluster::Instance().Get().Call<rpc_def::BindAccountAck, 256, const std::string&>(accountIdx, rpc_def::BIND_ACCOUNT, _userId);
	if (ack.errCode != 0) {
		rsp.errCode = ack.errCode;

		
		return false;
	}


	return true;
}
