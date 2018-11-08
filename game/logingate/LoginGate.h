#ifndef __LOGINGATE_H__
#define __LOGINGATE_H__
#include "hnet.h"

class LoginGate {
public:
	LoginGate() {}
	~LoginGate() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__LOGINGATE_H__
