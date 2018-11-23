#ifndef __ROLEMGR_H__
#define __ROLEMGR_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "rpcdefine.h"

class Role {
public:
	Role();
	~Role();

private:
};

class RoleMgr {
	typedef LockTable<int64_t, Role, spin_mutex, fake_mutex> RoleTable;

public:
	static RoleMgr& Instance() {
		static RoleMgr g_instance;
		return g_instance;
	}

	void Start();

private:
	RoleMgr() {}
	~RoleMgr() {}

private:
	RoleTable _roles;
};

#endif //__ROLE_CACHE_H__
