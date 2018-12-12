#ifndef __IDMGR_H__
#define __IDMGR_H__
#include "hnet.h"
#include "nodedefine.h"
#include "conf.h"
#include "spin_mutex.h"
#include "rpcdefine.h"

class IdMgr : public Conf {
public:
	IdMgr();
	~IdMgr() {}

private:
	bool Initialize();

	rpc_def::GetIdAck GetId();

	int32_t _poolSize;
	int32_t _batchSize;

	spin_mutex _mutex;
	int64_t _nextId;
	int64_t _saveId;
};

#endif //__IDMGR_H__
