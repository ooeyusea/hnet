#ifndef __SESSION_H__
#define __SESSION_H__
#include "hnet.h"
#include "websocket.h"

class Session {
public:
	Session(int32_t fd, const std::string& ip, int32_t port) : _socket(fd), _ip(ip), _port(port) {}
	~Session() {}

	void Start();

	bool Auth();
	bool BindAccount();

private:
	websocket::WebSocket _socket;
	std::string _ip;
	int32_t _port;

	std::string _userId;
};

#endif //__SESSION_H__
