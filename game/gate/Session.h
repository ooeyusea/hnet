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

	inline int32_t GetFd() const { return _socket.GetFd(); }
	inline const std::string& GetUserId() const { return _userId; }
	inline int32_t GetVersion() const { return _version; }

private:
	websocket::WebSocket _socket;
	std::string _ip;
	int32_t _port;
	
	bool _terminate = false;
	
	int64_t _aliveTick;

	int32_t _cacheIdx;
	int32_t _logicIdx;
	
	int32_t _version;
	std::string _userId;
	bool _hasRole = false;
	rpc_def::RoleInfo _role;
};

#endif //__SESSION_H__
