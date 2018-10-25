#ifndef __CORPC_H__
#define __CORPC_H__
#include "util.h"
#include "spin_mutex.h"
#include "coroutine.h"

namespace hyper_net {
	class CoRpcImpl {
	public:
		CoRpcImpl() {}
		~CoRpcImpl() {}

		bool DialRpc(const char * ip, const int32_t port);
		bool HandleRpc(const char * ip, const int32_t port);

		void Call(int32_t rpcId, const void * context, int32_t size, void * resContext, int32_t resSize);
		void Call(int32_t rpcId, const void * context, int32_t size);

	private:
		int32_t _fd;
	};
}

#endif // !__CORPC_H__
