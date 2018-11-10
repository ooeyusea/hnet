#ifndef _RPCDEFINE_H__
#define _RPCDEFINE_H__
#include "hnet.h"

namespace rpc_def {
	enum {
		BIND_ACCOUNT = 1,
	};

	struct BindAccountAck {
		int32_t errCode;
		std::string ip;
		int16_t port;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode != 0) {
				ar & ip;
				ar & port;
			}
		}
	};
}
#endif //__NODEDEFINE_H__
