#ifndef __LOGIC_H__
#define __LOGIC_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"

class Logic {
public:
	Logic() {}
	~Logic() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__ACCOUNT_H__
