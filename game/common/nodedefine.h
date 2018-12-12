#ifndef __NODEDEFINE_H__
#define __NODEDEFINE_H__
#include "hnet.h"

namespace node_def {
	enum {
		LOGINGATE = 1,
		GATE = 2,
		ACCOUNT = 3,
		LOGIC = 4,
		CACHE = 5,
		ID = 6,
		COUNT,
	};
}


#define ID_FROM_ZONE(zone, id) ((zone) << 5 | (id))
#define ZONE_FROM_ID(id) ((id) >> 5)
#define ID_FROM_ID(id) ((id) & 0x1F)
#define MAX_ZONE 0x07FF
#define MAX_ZONE_ID 0x1F

#endif //__NODEDEFINE_H__
