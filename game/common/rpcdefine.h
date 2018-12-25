#ifndef _RPCDEFINE_H__
#define _RPCDEFINE_H__
#include "hnet.h"
#include <vector>

namespace rpc_def {
	const int32_t BIND_ACCOUNT = 1;
	const int32_t KICK_USER = 2;
	const int32_t CLEAR_CACHE = 3;
	const int32_t LOGIN_ACCOUNT = 4;
	const int32_t LOGOUT_ACCOUNT = 5;
	const int32_t GATE_REPORT = 6;
	const int32_t LOAD_ACCOUNT = 7;
	const int32_t RECOVER_ACCOUNT = 8;
	const int32_t KILL_ACCOUNT_CACHE = 9;
	const int32_t CREATE_ACTOR = 10;
	const int32_t ACTIVE_ACTOR = 11;
	const int32_t DEACTIVE_ACTOR = 12;
	const int32_t DELIVER_MESSAGE = 13;
	const int32_t LOAD_ACTOR = 14;
	const int32_t SAVE_ACTOR = 15;
	const int32_t KILL_ACTOR_CACHE = 16;
	const int32_t REMOVE_ACTOR_CACHE = 17;
	const int32_t LANDING_ACTOR = 18;
	const int32_t KILL_ACTOR = 19;
	const int32_t TRIGGER_SAVE_ACTOR = 20;
	const int32_t REMOVE_ACTOR = 21;
	const int32_t GET_ID = 22;

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

	struct RoleCreater {
		std::string name;

		template <typename AR>
		void Archive(AR& ar) {
			ar & name;
		}
	};

	struct CreateRoleAck {
		int32_t errCode;
		int64_t roleId;

		template <typename AR>
		void Archive(AR& ar) {
			ar & errCode;
			if (errCode == 0) {
				ar & roleId;
			}
		}
	};

	struct KitchenWare {
		int32_t id;
		int32_t level;
		int64_t exp;

		template <typename AR>
		void Archive(AR& ar) {
			ar & id;
			ar & level;
			ar & exp;
		}
	};

	struct RoleData {
		std::string name;
		std::map<int32_t, std::vector<KitchenWare>> kitchenWares;

		template <typename AR>
		void Archive(AR& ar) {
			ar & name;
			ar & kitchenWares;
		}
	};

	struct Range {
		int64_t start;
		int64_t end;

		template <typename AR>
		void Archive(AR& ar) {
			ar & start;
			ar & end;
		}
	};

	template <typename T, typename Test, Test check>
	struct TestData {
		Test test;
		T data;

		template <typename AR>
		void Archive(AR& ar) {
			ar & test;
			if (test == check) {
				ar & data;
			}
		}
	};

	template <typename T>
	struct JustCallPtr {
		T * ptr;

		template <typename AR>
		void Archive(AR& ar) {
			int64_t tmp = (int64_t)ptr;
			ar & tmp;
			ptr = (T *)tmp;
		}
	};

	typedef rpc_def::TestData<rpc_def::TestData<rpc_def::RoleData, bool, true>, bool, true> KickPlayerAck;
	typedef rpc_def::TestData<Range, bool, true> GetIdAck;
}
#endif //__NODEDEFINE_H__
