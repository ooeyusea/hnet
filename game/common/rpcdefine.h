#ifndef _RPCDEFINE_H__
#define _RPCDEFINE_H__
#include "hnet.h"

namespace rpc_def {
	const int32_t BIND_ACCOUNT = 1;
	const int32_t KICK_USER = 2;
	const int32_t CLEAR_CACHE = 3;
	const int32_t LOGIN_ACCOUNT = 4;
	const int32_t LOGOUT_ACCOUNT = 5;
	const int32_t GATE_REPORT = 6;
	const int32_t LOAD_ACCOUNT = 7;
	const int32_t RESTORE_ACCOUNT = 8;
	const int32_t CREATE_ACTOR = 9;
	const int32_t ACTIVE_ACTOR = 10;
	const int32_t DEACTIVE_ACTOR = 11;
	const int32_t DELIVER_MESSAGE = 12;

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
