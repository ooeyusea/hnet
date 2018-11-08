#include "Session.h"
#include <chrono>
#include "util.h"
#include "websocket.h"
#include "rpc.h"
#include "nodedefine.h"
#include "servernode.h"
#include "rpcdefine.h"

void Session::Start() {
	if (!websocket::ShakeHands(_fd))
		return;

	if (!hn_test_fd(_fd))
		return;

	if (!Auth()) {
		hn_close(_fd);
		return;
	}

	if (!hn_test_fd(_fd))
		return;

	if (!BindAccount()) {
		hn_close(_fd);
		return;
	}

	hn_close(_fd);
}

bool Session::Auth() {

	return true;
}

bool Session::BindAccount() {
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);

	rpc_def::BindAccountAck ack = Cluster::Instance().Get().Call<rpc_def::BindAccountAck, 256>(accountIdx, rpc_def::BIND_ACCOUNT, _userId);
	if (ack.errCode != 0) {

		return false;
	}


	return true;
}
