#ifndef __CACHE_H__
#define __CACHE_H__
#include "hnet.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "nodedefine.h"

class Cache {
	struct Account {
		int16_t gate = 0;

	};
public:
	Cache() {}
	~Cache() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	LockTable<std::string, Account, spin_mutex> _accountCache;
};

#endif //__ACCOUNT_H__
