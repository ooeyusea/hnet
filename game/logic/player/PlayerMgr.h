#ifndef __PLAYERMGR_H__
#define __PLAYERMGR_H__
#include "hnet.h"
#include "nodedefine.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "rpcdefine.h"
#include <unordered_map>
#include "util.h"
#include "method.h"
#include "conf.h"

class Object;
class PlayerMgr : public method::MethodCallor<int32_t, Object&>, public Conf{
	struct Method {
		std::function<void(Object * obj, hn_iachiver & ar)> fn;
		char file[MAX_PATH_SIZE];
		int32_t line;
	};
public:
	PlayerMgr();
	~PlayerMgr() {}

private:
	bool Initialize();

	int32_t Active(int64_t actor, const std::string& userId, int32_t fd, int16_t gate);
	void Deactive(int64_t actor);
	void DealMessage(int64_t actor, hn_deliver_buffer buff);
	rpc_def::KickPlayerAck Kill(int64_t actor, int32_t reason);

	void StartSaveTimer(Object * obj);
	void StopSaveTimer(Object * obj);
	void Save(int64_t actor);
	bool SaveObject(Object * obj, bool remove);

	void Pack(Object * obj, rpc_def::RoleData& data);
	bool Read(Object * obj, rpc_def::RoleData& data);

	void StartRecoverTimer(Object * obj);
	void StopRecoverTimer(Object * obj);
	void Remove(int64_t actor, int64_t ticker);

	inline int64_t Order(int64_t actor) {
		return actor;
	}

	int32_t _saveInterval;
	int32_t _recoverTimeout;
};

extern PlayerMgr g_playerMgr;

#endif //__ROLE_CACHE_H__
