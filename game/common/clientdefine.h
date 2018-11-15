#ifndef _CLIENTDEFINE_H__
#define _CLIENTDEFINE_H__
#include "hnet.h"
#include <string>

namespace client_def {
	namespace c2s {
		const int32_t AUTH_REQ = 1;
		const int32_t LOGIN_REQ = 2;
		const int32_t CREATE_ROLE_REQ = 3;
		const int32_t SELECT_ROLE_REQ = 4;
	}

	namespace s2c {
		const int32_t AUTH_RSP = 1;
		const int32_t LOGIN_RSP = 2;
		const int32_t CREATE_ROLE_RSP = 3;
		const int32_t KICK_NTF = 4;
		const int32_t SELECT_ROLE_RSP= 5;
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

	struct SelectRoleReq {
		int64_t id;

		template <typename AR>
		void Archive(AR& ar) {
			ar & id;
		}
	};

	struct SelectRoleRsp {
		int32_t errCode;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
		}
	};
}
#endif //__NODEDEFINE_H__
