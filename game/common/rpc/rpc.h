#ifndef __RPC_H__
#define __RPC_H__
#include "hnet.h"

class RpcMgr {
public:
	static RpcMgr& Instance() {
		RpcMgr g_instance;
		return g_instance;
	}

	inline hn_rpc& Get() { return _rpc; }

private:
	RpcMgr() {}

	hn_rpc _rpc;
};

#endif // !__RPC_H__
