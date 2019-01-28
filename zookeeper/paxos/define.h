#ifndef __DEFINE_H__
#define __DEFINE_H__
#include "hnet.h"
#include "util.h"
#include <string>

#pragma pack(push,1)

namespace paxos_def {
	enum {
		PX_NONE = 0,
		PX_FOLLOW_INFO,
		PX_LEADER_INFO,
		PX_ACK_EPOCH,
		PX_TRUNC_DATASET,
		PX_SNAP_DATASET,
		PX_DIFF_DATASET,
		PX_UPDATED,
		PX_NEWLEADER,
		PX_ACK,
		PX_REQUEST,
		PX_PROPOSE,
		PX_ACK_PROPOSE,
		PX_COMMIT,
		PX_PING,
		PX_PONG,
	};

	struct FollowInfo {
		int8_t type;
		int32_t peerEpoch;
	};

	struct LeaderInfo {
		int8_t type;
		int32_t peerEpoch;
	};

	struct AckEpoch {
		int8_t type;
		int64_t lastZxId;
		int32_t peerEpoch;
	};

	struct Trunc {
		int8_t type;
		int64_t zxId;
	};

	struct Snap {
		int8_t type;
		int32_t size;
	};

	struct Request {
		int8_t type;
		int32_t size;
	};

	struct Propose {
		int8_t type;
		int64_t zxId;
		int32_t size;
	};

	struct ProposeAck {
		int8_t type;
		int64_t zxId;
	};

	struct Commit {
		int8_t type;
		int64_t zxId;
	};
}

#pragma pack(pop)
#endif //__DEFINE_H__
