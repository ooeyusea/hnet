#ifndef _RPCDEFINE_H__
#define _RPCDEFINE_H__
#include "hnet.h"

namespace rpc_def {
	enum {
		BIND_ACCOUNT = 1,
		KICK_USER = 2,
		CLEAR_CACHE = 3,
		LOGIN_ACCOUNT = 4,
		LOGOUT_ACCOUNT = 5,
		GATE_REPORT = 6,
		LOAD_ACCOUNT = 7,
		RESTORE_ACCOUNT = 8,
		CREATE_ACTOR = 9,
	};

	struct BindAccountAck {
		int32_t errCode;
		std::string ip;
		int16_t port;
		int64_t check;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode == 0) {
				ar & ip;
				ar & port;
				ar & check;
			}
		}
	};

	struct RoleInfo {
		int64_t id;
		std::string name;

		template <typename AR>
		void Archive(AR& ar) {
			ar & id;
			ar & name;
		}
	};

	struct LoadAccountAck {
		int32_t errCode;
		bool hasRole;
		RoleInfo role;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode == 0) {
				ar & hasRole;
				if (hasRole) {
					ar & role;
				}
			}
		}
	};

	struct CreateRoleAck {
		int32_t errCode;
		RoleInfo role;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode == 0) {
				ar & role;
			}
		}
	};
}
#endif //__NODEDEFINE_H__
