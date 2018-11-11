#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__
#include "hnet.h"

class Account {
public:
	Account() {}
	~Account() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__ACCOUNT_H__
