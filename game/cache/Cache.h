#ifndef __CACHE_H__
#define __CACHE_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "conf.h"

class Cache : public Conf {
public:
	Cache() {}
	~Cache() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	bool OpenDB();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;
};

#endif //__ACCOUNT_H__
