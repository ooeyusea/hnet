#ifndef __PLAYERMGR_H__
#define __PLAYERMGR_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "rpcdefine.h"
#include <unordered_map>

class Object;
class PlayerMgr {
public:
	PlayerMgr();
	~PlayerMgr() {}

	bool Init();

	int32_t Active(int64_t actor, const std::string& userId, int32_t fd, int16_t gate);
	void Deactive(int64_t actor);
	void DealMessage(int64_t actor, hn_deliver_buffer buff);
	rpc_def::TestData<rpc_def::RoleData, bool, true> Kill(int64_t actor, int32_t reason);

	inline int64_t Order(int64_t actor) {
		return actor;
	}

private:
	std::unordered_map<int32_t, std::function<void (Object * object, const char * message, int32_t size)>> _methods;
};

#endif //__ROLE_CACHE_H__
