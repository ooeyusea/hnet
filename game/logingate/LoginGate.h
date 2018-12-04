#ifndef __LOGINGATE_H__
#define __LOGINGATE_H__
#include "hnet.h"
#include "conf.h"

class LoginGate : public Conf {
public:
	LoginGate();
	~LoginGate() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	bool ReadConf();

private:
	int32_t _listenPort = 0;
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	int16_t _loginGateId;
};

#endif //__LOGINGATE_H__
