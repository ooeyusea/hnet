#ifndef _RPCDEFINE_H__
#define _RPCDEFINE_H__
#include "hnet.h"

namespace rpc_def {
	enum {
		BIND_ACCOUNT = 1,
		KICK_USER = 2,
		CLEAR_CACHE = 3,
		CHECK_ACCOUNT = 4,
		GATE_REPORT = 5,
	};

	struct BindAccountAck {
		int32_t errCode;
		std::string ip;
		int16_t port;
		int64_t check;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode != 0) {
				ar & ip;
				ar & port;
				ar & check;
			}
		}
	};
}
#endif //__NODEDEFINE_H__
