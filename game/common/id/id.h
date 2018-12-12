#ifndef __ID_H__
#define __ID_H__
#include "util.h"
#include "node/servernode.h"
#include "rpcdefine.h"
#include "nodedefine.h"

class IdGeter  {
public:
	static IdGeter& Instance() {
		static IdGeter g_instance;
		return g_instance;
	}

	int64_t Get() {
		if (_start == 0 || _start >= _end) {
			if (!RemoteGet())
				return 0;
		}

		return _start++;
	}

private:
	IdGeter() {}
	~IdGeter() {}

	bool RemoteGet() {
		try {
			int16_t id = Cluster::Instance().GetId();
			int32_t idIndex = Cluster::Instance().ServiceId(node_def::ID, ID_FROM_ZONE(ZONE_FROM_ID(id), 1));

			rpc_def::GetIdAck ack = Cluster::Instance().Get().Call(idIndex).Do<rpc_def::GetIdAck, 128>(rpc_def::GET_ID);
			if (!ack.test)
				return false;

			_start = ack.data.start;
			_end = ack.data.end;
		}
		catch (hn_rpc_exception& e) {
			hn_error("Get id rpc failed : {}", e.what());
			return false;
		}

		return true;
	}

private:
	int64_t _start = 0;
	int64_t _end = 0;
};

#endif //__ID_H__
