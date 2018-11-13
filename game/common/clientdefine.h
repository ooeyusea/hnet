#ifndef _CLIENTDEFINE_H__
#define _CLIENTDEFINE_H__
#include "hnet.h"
#include <string>

namespace client_def {
	namespace c2s {
		enum {
			AUTH_REQ = 1,
		};
	}

	namespace s2c {
		enum {
			AUTH_RSP = 1,
		};
	}

	struct AuthReq {
		std::string auth;

		template <typename AR>
		void Archive(AR& ar) {
			ar & auth;
		}
	};

	struct AuthRsp {
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
			}
		}
	};
}
#endif //__NODEDEFINE_H__
