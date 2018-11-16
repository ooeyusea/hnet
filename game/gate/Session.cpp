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
		int32_t errCode = Cluster::Instance().Get().Call<int32_t, 256, int16_t, const std::string&>(accountIdx, hn_rpc_order{ util::CalcUniqueId(_userId.c_str()) }, 
			rpc_def::LOGIN_ACCOUNT, id, _userId, req.check, _socket.GetFd());
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
	Cluster::Instance().Get().Call<256, int16_t, const std::string&>(accountIdx, hn_rpc_order{ util::CalcUniqueId(_userId.c_str()) }, rpc_def::LOGOUT_ACCOUNT, id, _userId, _socket.GetFd());
}

bool Session::LoadAccount() {
	int16_t id = Cluster::Instance().GetId();
	int16_t cache = Zone::Instance().Calc(node_def::CACHE, ZONE_FROM_ID(id), _userId);
	_cacheIdx = Cluster::Instance().ServiceId(node_def::CACHE, ID_FROM_ZONE(ZONE_FROM_ID(id), cache));

	try {
		rpc_def::LoadAccountAck ack = Cluster::Instance().Get().Call<rpc_def::LoadAccountAck, 256, const std::string&>(_cacheIdx, hn_rpc_order{ util::CalcUniqueId(_userId.c_str()) }, 
			rpc_def::LOAD_ACCOUNT, _userId, id, _socket.GetFd());
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

bool Session::CreateRole() {
	while (true) {
		client_def::CreateRoleReq req;
		if (!_socket.Read(client_def::c2s::CREATE_ROLE_REQ, _version, req))
			return false;

		try {
			rpc_def::CreateRoleAck ack = Cluster::Instance().Get().Call<rpc_def::CreateRoleAck, 256, const std::string&, const std::string&>(
				_cacheIdx, hn_rpc_order{ util::CalcUniqueId(_userId.c_str()) }, rpc_def::CREATE_ACTOR, _userId, req.name);

			if (ack.errCode != 0) {
				client_def::CreateRoleRsp rsp;
				rsp.errCode = ack.errCode;

				_socket.Write<128>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
				continue;
			}

			_hasRole = true;
			_role = ack.role;

			client_def::CreateRoleRsp rsp;
			rsp.errCode = err_def::NONE;
			rsp.role.id = _role.id;
			rsp.role.name = _role.name;

			_socket.Write<256>(client_def::s2c::CREATE_ROLE_RSP, _version, rsp);
			break;
		}
		catch (hn_rpc_exception& e) {
			client_def::LoginRsp rsp;
			rsp.errCode = err_def::CREATE_ROLE_TIMEOUT;

			_socket.Write<128>(client_def::s2c::LOGIN_RSP, 0, rsp);
			return false;
		}
	}

	return CheckConnect();
}

bool Session::LoginRole() {
	client_def::SelectRoleReq req;
	if (!_socket.Read(client_def::c2s::SELECT_ROLE_REQ, _version, req))
		return false;

	if (req.id != _role.id)
		return false;

	int16_t id = Cluster::Instance().GetId();
	int16_t logic = Zone::Instance().Calc(node_def::LOGIC, ZONE_FROM_ID(id), _userId);
	_logicIdx = Cluster::Instance().ServiceId(node_def::LOGIC, ID_FROM_ZONE(ZONE_FROM_ID(id), req.id));

	try {
		client_def::SelectRoleRsp rsp;
		rsp.errCode = Cluster::Instance().Get().Call<int32_t, 256, int16_t, const std::string&>(_logicIdx, hn_rpc_order{ _role.id }, rpc_def::ACTIVE_ACTOR, id, _userId, _socket.GetFd(), req.id);

		_socket.Write<128>(client_def::s2c::SELECT_ROLE_RSP, _version, rsp);

		return rsp.errCode == 0;
	}
	catch (hn_rpc_exception& e) {
		client_def::LoginRsp rsp;
		rsp.errCode = err_def::SELECT_ROLE_TIMEOUT;

		_socket.Write<128>(client_def::s2c::SELECT_ROLE_RSP, 0, rsp);
		return false;
	}

	return CheckConnect();
}

void Session::LogoutRole() {
	int16_t id = Cluster::Instance().GetId();
	Cluster::Instance().Get().Call<256, int16_t>(_logicIdx, hn_rpc_order{ _role.id }, rpc_def::DEACTIVE_ACTOR, _role.id);
}

void Session::DealPacket() {
	int32_t size = 0;
	const char * data = _socket.ReadFrame(size);
	while (data && size > 0 && size <= MAX_PACKET_SIZE) {
		int16_t id = Cluster::Instance().GetId();
		Cluster::Instance().Get().Call<MAX_PACKET_SIZE, int16_t>(_logicIdx, hn_rpc_order{ _role.id }, rpc_def::DELIVER_MESSAGE, _role.id, hn_deliver_buffer{ data, size });

		data = _socket.ReadFrame(size);
	}
}
