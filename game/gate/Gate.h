#ifndef __GATE_H__
#define __GATE_H__
#include "hnet.h"

class Gate {
public:
	Gate() {}
	~Gate() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__GATE_H__
