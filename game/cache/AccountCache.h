#ifndef __ACCOUNT_CACHE_H__
#define __ACCOUNT_CACHE_H__
#include "hnet.h"
#include "util.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "rpcdefine.h"

class Account {
public:
	bool Load();
	bool Kick();
	bool TransforTo(int16_t zone);
	void Pack(rpc_def::LoadAccountAck& ack);

private:
	int16_t gate = 0;
	int32_t fd = 0;
	bool loadData = false;
};

class AccountCache {
	typedef LockTable<std::string, Account, spin_mutex, fake_mutex> AccountTable;

public:
	static AccountCache& Instance() {
		static AccountCache g_instance;
		return g_instance;
	}

	void Start();

private:
	AccountCache() {}
	~AccountCache() {}

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	AccountTable _accounts;
};

#endif //__ACCOUNT_CACHE_H__
