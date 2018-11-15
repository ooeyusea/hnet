#ifndef __ERRORDEFINE_H__
#define __ERRORDEFINE_H__
#include "hnet.h"

namespace err_def {
	enum {
		NONE = 0,
		INVALID_USER = 1,
		KICK_FAILED = 2,
		NO_GATE = 3,
		AUTH_TIMEOUT = 4,
		LOGIN_TIMEOUT = 5,
		LOAD_ACCOUNT_TIMEOUT = 6,
		LOGIN_ACTOR_TIMEOUT = 7,
		OTHER_USER_LOGIN = 8,
	};
}
#endif //__NODEDEFINE_H__
