#ifndef __ROLE_CACHE_H__
#define __ROLE_CACHE_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "rpcdefine.h"

class Role {
public:
	Role();
	~Role();

	bool Load(int64_t roleId);

	void StartLanding(int64_t roleId, int64_t timeout);
	bool Landing(int64_t roleId);
	inline void StopLanding() {
		if (_landingTicker) {
			_landingTicker->Stop();
			_landingTicker = nullptr;
		}
	}

	void StartRecover(int64_t roleId, int64_t timeout);
	inline bool CheckRecover(hn_ticker * ticker) {
		return _recoverTicker == ticker;
	}

	inline void StopRecover() {
		if (_recoverTicker) {
			_recoverTicker->Stop();
			_recoverTicker = nullptr;
		}
	}

	void Pack(rpc_def::RoleData & data);
	void Save(const rpc_def::RoleData & data);

	bool Kick(int64_t roleId);
	inline void SetLogic(bool inLogic) { _inLogic = inLogic; }

private:
	bool _loadData = false;
	bool _inLogic = false;

	std::string _name;

	hn_ticker * _recoverTicker = nullptr;
	hn_ticker * _landingTicker = nullptr;
};

class RoleCache {
	typedef LockTable<int64_t, Role, spin_mutex, fake_mutex> RoleTable;

public:
	static RoleCache& Instance() {
		static RoleCache g_instance;
		return g_instance;
	}

	void Start(int32_t saveInterval, int32_t recoverTimeout);

private:
	RoleCache() {}
	~RoleCache() {}

	bool TransforTo(int64_t _roleId, int16_t zone);

private:
	RoleTable _roles;
	int32_t _saveInterval;
	int32_t _recoverTimeout;
};

#endif //__ROLE_CACHE_H__
