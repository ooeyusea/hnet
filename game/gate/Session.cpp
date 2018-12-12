#include "Session.h"
#include "clientdefine.h"
#include "servernode.h"
#include "nodedefine.h"
#include "errordefine.h"
#include "zone.h"

#define MAX_PACKET_SIZE 8192

void Session::Start() {
	auto co = util::DoWork([this] { CheckAlive(); });

	do {
		if (!_socket.ShakeHands())
			break;

		if (!CheckConnect())
			break;

		hn_trace("session [{}=>{}:{}] connected", _socket.GetFd(), _ip, _port);

		if (!Login())
			break;

		do {
			if (!LoadAccount())
				break;

			do {
				if (!_hasRole) {
					hn_info("session {}:{} has no role", _socket.GetFd(), _userId);

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

	hn_trace("session [{}=>{}:{}] timeout", _socket.GetFd(), _ip, _port);
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
	if (!_socket.Read(client_def::c2s::LOGIN_REQ, 0, req)) {
		hn_trace("session {} read login req failed", _socket.GetFd());
		return false;
	}

	_userId = std::move(req.userId);
	_version = req.version;

	hn_info("session {} read req {}:{}", _socket.GetFd(), _userId, req.check);

	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);

	try {
		int32_t errCode = Cluster::Instance().Get().Call(accountIdx).Do<int32_t, 256, const std::string&>(rpc_def::LOGIN_ACCOUNT, _userId, id, req.check, _socket.GetFd());
		if (errCode != 0) {
			client_def::LoginRsp rsp;
			rsp.errCode = errCode;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);

			hn_info("session {} login account {}:{} failed", _socket.GetFd(), _userId, req.check);
			return false;
		}
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::LOGIN_TIMEOUT;

		_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
		hn_error("session {} login account {}:{} rpc failed {}", _socket.GetFd(), _userId, req.check, e.what());
		return false;
	}

	return CheckConnect();
}

void Session::Logout() {
	int16_t id = Cluster::Instance().GetId();
	int32_t accountIdx = Cluster::Instance().ServiceId(node_def::ACCOUNT, 1);

	try {
		Cluster::Instance().Get().Call(accountIdx).Do<256, const std::string&>(rpc_def::LOGOUT_ACCOUNT, _userId, id, _socket.GetFd());

		hn_info("session {} logout {}", _socket.GetFd(), _userId);
	}
	catch (hn_rpc_exception& e) {
		hn_error("session {} logout {} rpc failed {}", _socket.GetFd(), _userId, e.what());
	}
}

bool Session::LoadAccount() {
	int16_t id = Cluster::Instance().GetId();
	int16_t cache = Zone::Instance().Calc(node_def::CACHE, ZONE_FROM_ID(id), _userId);
	_cacheIdx = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(ZONE_FROM_ID(id), cache));

	try {
		rpc_def::LoadAccountAck ack = Cluster::Instance().Get().Call(_cacheIdx).Do<rpc_def::LoadAccountAck, 256, const std::string&>(rpc_def::LOAD_ACCOUNT, _userId, id, _socket.GetFd());
		if (ack.errCode != 0) {
			client_def::LoginRsp rsp;
			rsp.errCode = ack.errCode;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);

			hn_error("session {}:{} load account failed", _socket.GetFd(), _userId);
			return false;
		}

		_hasRole = ack.hasRole;
		if (_hasRole)
			_role = ack.role;

		hn_info("session {}:{} load account success", _socket.GetFd(), _userId);
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::LOAD_ACCOUNT_TIMEOUT;

		_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);

		hn_error("session {}:{} load account rpc failed {}", _socket.GetFd(), _userId, e.what());
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
	try {
		 Cluster::Instance().Get().Call(_cacheIdx).Do<256, const std::string&>(rpc_def::RECOVER_ACCOUNT, _userId, id, _socket.GetFd());
	}
	catch (hn_rpc_exception& e) {
		hn_error("session {}:{} recover account rpc failed {}", _socket.GetFd(), _userId, e.what());
	}
}

bool Session::CreateRole() {
	while (true) {
		client_def::CreateRoleReq req;
		if (!_socket.Read(client_def::c2s::CREATE_ROLE_REQ, _version, req)) {
			hn_trace("session {}:{} read create role failed", _socket.GetFd(), _userId);
			return false;
		}

		hn_trace("session {}:{} create role {}", _socket.GetFd(), _userId, req.name);

		try {
			rpc_def::RoleCreater creator;
			creator.name = req.name;

			rpc_def::CreateRoleAck ack = Cluster::Instance().Get().Call(_cacheIdx).Do<rpc_def::CreateRoleAck, 256, const std::string&>(rpc_def::CREATE_ACTOR, _userId, creator);

			if (ack.errCode != 0) {
				client_def::CreateRoleRsp rsp;
				rsp.errCode = ack.errCode;

				_socket.Write<128>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
				hn_trace("session {}:{} create role {} failed", _socket.GetFd(), _userId, req.name);
				continue;
			}

			_hasRole = true;
			_role.id = ack.roleId;
			_role.name = req.name;

			client_def::CreateRoleRsp rsp;
			rsp.errCode = err_def::NONE;
			rsp.role.id = _role.id;
			rsp.role.name = _role.name;

			_socket.Write<256>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
			hn_info("session {}:{} create role {} success", _socket.GetFd(), _userId, req.name);
			break;
		}
		catch (hn_rpc_exception& e) {
			client_def::LoginRsp rsp;
			rsp.errCode = err_def::CREATE_ROLE_TIMEOUT;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);

			hn_error("session {}:{} create role {} rpc failed {}", _socket.GetFd(), _userId, req.name, e.what());
			return false;
		}
	}

	return CheckConnect();
}

bool Session::LoginRole() {
	client_def::SelectRoleReq req;
	if (!_socket.Read(client_def::c2s::SELECT_ROLE_REQ, _version, req)) {
		hn_trace("session {}:{} read login role {} failed", _socket.GetFd(), _userId, req.id);
		return false;
	}

	if (req.id != _role.id) {
		hn_trace("session {}:{} login role {}/{} failed", _socket.GetFd(), _userId, req.id, _role.id);
		return false;
	}

	int16_t id = Cluster::Instance().GetId();
	int16_t logic = Zone::Instance().Calc(node_def::LOGIC, ZONE_FROM_ID(id), req.id);
	_logicIdx = Cluster::Instance().ServiceId(node_def::LOGIC, ID_FROM_ZONE(ZONE_FROM_ID(id), logic));

	try {
		client_def::SelectRoleRsp rsp;
		rsp.errCode = Cluster::Instance().Get().Call(_logicIdx).Do<int32_t, 256, int64_t, const std::string&>(rpc_def::ACTIVE_ACTOR, req.id, _userId, _socket.GetFd(), id);

		_socket.Write<128>(client_def::s2c::SELECT_ROLE_RSP, _version, rsp);

		hn_info("session {}:{} login role {} result {}", _socket.GetFd(), _userId, req.id, rsp.errCode);
		return rsp.errCode == 0;
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::SELECT_ROLE_TIMEOUT;

		_socket.Write<128>(client_def::s2c::SELECT_ROLE_RSP, 0, rsp);
		hn_error("session {}:{} login role {} rpc failed {}", _socket.GetFd(), _userId, req.id, e.what());
		return false;
	}

	return CheckConnect();
}

void Session::LogoutRole() {
	int16_t id = Cluster::Instance().GetId();
	try {
		Cluster::Instance().Get().Call(_logicIdx).Do<256>(rpc_def::DEACTIVE_ACTOR, _role.id);
		hn_info("session {}:{} logout role {}", _socket.GetFd(), _userId, _role.id);
	}
	catch (hn_rpc_exception& e) {
		hn_error("session {}:{} logout role {} rpc failed {}", _socket.GetFd(), _userId, _role.id, e.what());
	}
}

void Session::DealPacket() {
	int32_t size = 0;
	const char * data = _socket.ReadFrame(size);
	while (data && size > 0 && size <= MAX_PACKET_SIZE) {
		int16_t id = Cluster::Instance().GetId();
		try {
			Cluster::Instance().Get().Call(_logicIdx).Do<128>(rpc_def::DELIVER_MESSAGE, _role.id, hn_deliver_buffer{ data, size });
		}
		catch (hn_rpc_exception& e) {
			hn_info("session {}:{}:{} deliver message rpc failed {} ", _socket.GetFd(), _userId, _role.id, e.what());
		}

		data = _socket.ReadFrame(size);
	}
}
