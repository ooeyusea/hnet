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
	bool Load(const std::string& userId);
	bool Kick(const std::string& userId);

	inline void Set(int16_t gate, int32_t fd) {
		_gate = gate;
		_fd = fd;
	}
	inline int16_t GetGate() const { return _gate; }
	inline int32_t GetFd() const { return _fd; }
	
	inline bool HasRole() const { return _hasRole; }
	int64_t CreateRole(const std::string& userId, const rpc_def::RoleCreater& creator);

	void Pack(rpc_def::LoadAccountAck& ack);

	void StartRecover(std::string userId, int64_t elapse);
	inline void StopRecover() {
		if (_ticker) {
			_ticker->Stop();
			_ticker = nullptr;
		}
	}

private:
	int16_t _gate = 0;
	int32_t _fd = -1;
	bool _loadData = false;
	bool _hasRole = false;
	rpc_def::RoleInfo _role;

	hn_ticker * _ticker = nullptr;
};

class AccountCache {
	typedef LockTable<std::string, Account, spin_mutex, fake_mutex> AccountTable;

public:
	static AccountCache& Instance() {
		static AccountCache g_instance;
		return g_instance;
	}

	void Start(int32_t accountTimeout);

private:
	AccountCache() {}
	~AccountCache() {}

	bool TransforTo(const std::string& userId, int16_t zone);

private:
	AccountTable _accounts;

	int32_t _accountTimeout;
};

#endif //__ACCOUNT_CACHE_H__
