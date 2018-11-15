#include "Session.h"
#include "clientdefine.h"
#include "servernode.h"
#include "nodedefine.h"
#include "errordefine.h"
#include "zone.h"

void Session::Start() {
	auto co = util::DoWork([this] { CheckAlive(); });

	do {
		if (!_socket.ShakeHands())
			break;

		if (!CheckConnect())
			break;

		if (!Login())
			break;

		do {
			if (!LoadAccount())
				break;

			do {
				if (!_hasRole) {
					if (!CreateRole())
						break;
				}

				if (!LoginRole())
					break;

				util::DoWork([this] { DealPacket(); }).Wait();

				LogoutRole();

			} while (false);

			RecoverAccount();

		} while (false);

		Logout();

	} while (false);

	StopCheckAlive(co);
}

void Session::CheckAlive() {
	_aliveTick = util::GetTickCount();
	while (!_terminate) {
		hn_sleep 1000;

		int64_t now = util::GetTickCount();
		if (now - _aliveTick >= 30000)
			break;
	}

	_socket.Shutdown();
}

void Session::StopCheckAlive(util::WaitCo & co) {
	_terminate = true;
	co.Wait();
}

bool Session::CheckConnect() {
	if (!_socket.IsConnected())
		return false;

	_aliveTick = util::GetTickCount();
	return true;
}

bool Session::Login() {
	client_def::LoginReq req;
	if (!_socket.Read(client_def::c2s::LOGIN_REQ, 0, req))
		return false;

	_userId = std::move(req.userId);
	_version = req.version;

	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);

	try {
		int32_t errCode = Cluster::Instance().Get().Call<int32_t, 256, int16_t, const std::string&>(accountIdx, rpc_def::LOGIN_ACCOUNT, id, _userId, req.check, _socket.GetFd());
		if (errCode != 0) {
			client_def::LoginRsp rsp;
			rsp.errCode = errCode;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
			return false;
		}
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::LOGIN_TIMEOUT;

		_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
		return false;
	}

	return CheckConnect();
}

void Session::Logout() {
	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);
	Cluster::Instance().Get().Call<256, int16_t, const std::string&>(accountIdx, rpc_def::LOGOUT_ACCOUNT, id, _userId, _socket.GetFd());
}

bool Session::LoadAccount() {
	int16_t id = Cluster::Instance().GetId();
	int16_t cache = Zone::Instance().Calc(node_def::CACHE, ZONE_FROM_ID(id), _userId);
	_cacheIdx = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(ZONE_FROM_ID(id), cache));

	try {
		rpc_def::LoadAccountAck ack = Cluster::Instance().Get().Call<rpc_def::LoadAccountAck, 256, int16_t, const std::string&>(_cacheIdx, rpc_def::LOAD_ACCOUNT, id, _userId, _socket.GetFd());
		if (ack.errCode != 0) {
			client_def::LoginRsp rsp;
			rsp.errCode = ack.errCode;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
			return false;
		}

		_hasRole = ack.hasRole;
		if (_hasRole)
			_role = ack.role;
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::LOAD_ACCOUNT_TIMEOUT;

		_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
		return false;
	}

	if (!CheckConnect())
		return false;

	client_def::LoginRsp rsp;
	rsp.errCode = err_def::NONE;
	rsp.hasRole = _hasRole;
	if (_hasRole) {
		rsp.role.id = _role.id;
		rsp.role.name = _role.name;
	}

	_socket.Write<128>(client_def::s2c::LOGIN_RSP, _version, rsp);
	return true;
}

void Session::RecoverAccount() {
	int16_t id = Cluster::Instance().GetId();
	Cluster::Instance().Get().Call<256, int16_t, const std::string&>(_cacheIdx, rpc_def::RESTORE_ACCOUNT, id, _userId, _socket.GetFd());
}

bool Session::CreateRole() {
	client_def::CreateRoleReq req;
	if (!_socket.Read(client_def::c2s::CREATE_ROLE_REQ, _version, req))
		return false;

	try {
		int16_t id = Cluster::Instance().GetId();
		rpc_def::CreateRoleAck ack = Cluster::Instance().Get().Call<rpc_def::CreateRoleAck, 256, int16_t, const std::string&, int32_t, const std::string&>(
			_cacheIdx, rpc_def::CREATE_ACTOR, id, _userId, _socket.GetFd(), req.name);
		if (ack.errCode != 0) {
			client_def::CreateRoleRsp rsp;
			rsp.errCode = ack.errCode;

			_socket.Write<128>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
			return false;
		}

		_hasRole = true;
		_role = ack.role;

		client_def::CreateRoleRsp rsp;
		rsp.errCode = err_def::NONE;
		rsp.role.id = _role.id;
		rsp.role.name = _role.name;

		_socket.Write<256>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::LOAD_ACCOUNT_TIMEOUT;

		_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
		return false;
	}

	return CheckConnect();
}

bool Session::LoginRole() {
	return CheckConnect();
}

void Session::LogoutRole() {

}


void Session::DealPacket() {
	while (true) {
		
	}
}

