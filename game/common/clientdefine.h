#ifndef _CLIENTDEFINE_H__
#define _CLIENTDEFINE_H__
#include "hnet.h"
#include <string>

namespace client_def {
	namespace c2s {
		enum {
			AUTH_REQ = 1,
			LOGIN_REQ = 2,
			CREATE_ROLE_REQ = 3,
		};
	}

	namespace s2c {
		enum {
			AUTH_RSP = 1,
			LOGIN_RSP = 2,
			CREATE_ROLE_RSP = 3,
			KICK_NTF = 4,
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
			if (errCode == 0) {
				ar & ip;
				ar & port;
			}
		}
	};

	struct LoginReq {
		std::string userId;
		int64_t check;
		int32_t version;

		template <typename AR>
		void Archive(AR& ar) {
			ar & userId;
			ar & check;
			ar & version;
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

	struct LoginRsp {
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

	struct CreateRoleReq {
		std::string name;

		template <typename AR>
		void Archive(AR& ar) {
			ar & name;
		}
	};

	struct CreateRoleRsp {
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

	struct KickNtf {
		int32_t errCode;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
		}
	};
}
#endif //__NODEDEFINE_H__
