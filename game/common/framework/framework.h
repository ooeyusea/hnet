#ifndef __FRAMEWORK_H__
#define __FRAMEWORK_H__
#include "hnet.h"
#include "nodedefine.h"

class Framework {
public:
	Framework() {}
	~Framework() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__FRAMEWORK_H__
