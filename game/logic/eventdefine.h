#ifndef __EVENT_DEFINE_H__
#define __EVENT_DEFINE_H__
#include "util.h"
#include "Object.h"
#include "rpcdefine.h"

namespace event_def {
	const int32_t OBJECT_CREATE = 1;
	const int32_t OBJECT_DESTROY = 2;
	const int32_t PLAYER_CREATE = 3;
	const int32_t PLAYER_DESTROY = 4;

	const int32_t PARSE_DATA = 5;
	const int32_t PLAYER_LOAD_COMPLETE = 6;
	const int32_t PACK_DATA = 7;

	const int32_t PLAYER_CONNECT = 8;
	const int32_t PLAYER_DISCONNECT = 9;

	struct Data {
		Object& obj;
		rpc_def::RoleData& data;
	};
}

#endif //__EVENT_DEFINE_H__