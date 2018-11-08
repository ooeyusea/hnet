#ifndef __SESSION_H__
#define __SESSION_H__
#include "hnet.h"

struct GameObjectMail {
	int8_t type;
	int32_t opCode;
	int32_t size;
};

class Session {
public:
	Session(int32_t fd, const std::string& ip, int32_t port) : _fd(fd), _ip(ip), _port(port) {}
	~Session() {}

	void Start();

	bool Auth();
	bool BindAccount();

private:
	int32_t _fd;
	std::string _ip;
	int32_t _port;

	std::string _userId;
};

#endif //__SESSION_H__
