#ifndef __SESSION_H__
#define __SESSION_H__
#include "hnet.h"
#include "websocket.h"
#include "util.h"
#include "rpcdefine.h"

class Session {
public:
	Session(int32_t fd, const std::string& ip, int32_t port) : _socket(fd), _ip(ip), _port(port) {}
	~Session() {}

	void Start();

	void CheckAlive();
	void StopCheckAlive(util::WaitCo & co);
	bool CheckConnect();

	bool Login();
	void Logout();
	bool LoadAccount();
	void RecoverAccount();
	bool CreateRole();
	bool LoginRole();
	void LogoutRole();

	void DealPacket();

	template <typename T>
	bool Read(int32_t msgId, int32_t version, T& t) {
		int32_t size = 0;
		const char * data = _socket.ReadFrame(size);
		if (data == nullptr)
			return false;

		if (*(int32_t*)data != msgId)
			return false;

		hn_istream stream(data, size);
		hn_iachiver ar(stream, 0);

		ar >> t;
		if (ar.Fail())
			return false;

		return true;
	}

	template <int32_t size, typename T>
	void Send(int32_t msgId, int32_t version, T& t) {
		char buff[size];
		hn_ostream stream(buff, size);
		hn_oachiver ar(stream, version);
		ar << t;

		if (!ar.Fail())
			_socket.SendFrame(buff, stream.size());
	}

private:
	websocket::WebSocket _socket;
	std::string _ip;
	int32_t _port;
	
	bool _terminate = false;
	
	int64_t _aliveTick;

	int32_t _cacheIdx;
	
	int32_t _version;
	std::string _userId;
	bool _hasRole = false;
	rpc_def::RoleInfo _role;
};

#endif //__SESSION_H__
