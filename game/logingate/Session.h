#ifndef __SESSION_H__
#define __SESSION_H__
#include "hnet.h"
#include "websocket.h"

#define MAX_SESSION_BUFF_SIZE 8192

struct GameObjectMail {
	int8_t type;
	int32_t opCode;
	int32_t size;
};

class Session {
public:
	Session(int32_t fd, const std::string& ip, int32_t port) : _socket(fd), _ip(ip), _port(port) {}
	~Session() {}

	void Start();

	bool Auth();
	bool BindAccount();

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

	std::string _userId;
};

#endif //__SESSION_H__
