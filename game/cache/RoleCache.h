#ifndef __ROLE_CACHE_H__
#define __ROLE_CACHE_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"

class Role {
public:
private:

};

class RoleCache {
	typedef LockTable<int64_t, Role, spin_mutex, fake_mutex> RoleTable;

public:
	static RoleCache& Instance() {
		static RoleCache g_instance;
		return g_instance;
	}

	void Start();

private:
	RoleCache() {}
	~RoleCache() {}

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	RoleTable _accounts;
};

#endif //__ROLE_CACHE_H__
