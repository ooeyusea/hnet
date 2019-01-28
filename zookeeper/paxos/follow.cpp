#include "follow.h"
#include "define.h"
#include "socket_helper.h"

#define FIRST_SLEEP_INTERVAL 50
#define TRY_COUNT 5

Follow::Follow() {

}

Follow::~Follow() {

}

int32_t Follow::Following(int32_t id, int32_t peerEpoch, DataSet& dataset, const std::string& leaderIp, int32_t leaderPort) {
	int32_t fd = -1;
	for (int32_t i = 0; i < TRY_COUNT; ++i) {
		hn_sleep FIRST_SLEEP_INTERVAL;

		fd = hn_connect(leaderIp.c_str(), leaderPort);
		if (fd > 0)
			break;
	}

	if (fd < 0)
		return peerEpoch;

	hn_send(fd, (const char*)&id, sizeof(id));

	try {
		RegisterToLeader(fd, id, dataset, peerEpoch);

		SyncWithLeader(fd, id, dataset);

		ProcossPropose(fd, id, dataset);
	}
	catch (std::exception& e) {
		hn_error("follow exception: {}", e.what());
	}

	return peerEpoch;
}

void Follow::RegisterToLeader(int32_t fd, int32_t id, DataSet& dataset, int32_t& peerEpoch) {
	paxos_def::FollowInfo info = { paxos_def::PX_FOLLOW_INFO, peerEpoch };
	hn_send(fd, (const char*)&info, sizeof(info));

	paxos_def::LeaderInfo leaderInfo;
	SocketReader().ReadMessage<int8_t>(fd, paxos_def::PX_LEADER_INFO, leaderInfo);

	if (peerEpoch < leaderInfo.peerEpoch) {
		peerEpoch = leaderInfo.peerEpoch;
	}
	else if (peerEpoch > leaderInfo.peerEpoch) {
		throw std::logic_error("self peer epoch is bigger than leader");
	}

	paxos_def::AckEpoch ack = { paxos_def::PX_ACK_EPOCH, dataset.GetZxId(), peerEpoch };
	hn_send(fd, (const char*)&ack, sizeof(ack));
}

void Follow::SyncWithLeader(int32_t fd, int32_t id, DataSet& dataset) {
	int8_t type;
	SocketReader().ReadType(fd, type);

	switch (type) {
	case paxos_def::PX_TRUNC_DATASET: {
			paxos_def::Trunc trunc = { paxos_def::PX_TRUNC_DATASET };
			SocketReader().ReadRest<int8_t>(fd, trunc);

			dataset.Trunc(trunc.zxId);
		}
		break;
	case paxos_def::PX_SNAP_DATASET: {
			paxos_def::Snap snap = { paxos_def::PX_SNAP_DATASET };
			SocketReader().ReadRest<int8_t>(fd, snap);
			std::string data = SocketReader().ReadBlock(fd, snap.size);

			dataset.Snap(std::move(data));
		}
		break;
	case paxos_def::PX_DIFF_DATASET: break;
	}

	bool synced = false;
	while (!synced) {
		SocketReader().ReadType(fd, type);

		switch (type) {
		case paxos_def::PX_UPDATED: 
			synced = true;
			break;
		case paxos_def::PX_NEWLEADER: {
				int8_t ack = paxos_def::PX_ACK;
				hn_send(fd, (const char *)&ack, sizeof(ack));
			}
			break;
		case paxos_def::PX_PROPOSE: {
				paxos_def::Propose propose = { paxos_def::PX_PROPOSE };
				SocketReader().ReadRest<int8_t>(fd, propose);
				std::string data = SocketReader().ReadBlock(fd, propose.size);

				dataset.Propose(propose.zxId, std::move(data));
			}
			break;
		case paxos_def::PX_COMMIT: {
				paxos_def::Commit commit = { paxos_def::PX_COMMIT };
				SocketReader().ReadRest<int8_t>(fd, commit);

				dataset.Commit(commit.zxId);
			}
			break;
		}
	}
}

void Follow::ProcossPropose(int32_t fd, int32_t id, DataSet& dataset) {
	while (true) {
		int8_t type;
		SocketReader().ReadType(fd, type);

		switch (type) {
		case paxos_def::PX_PROPOSE: {
				paxos_def::Propose propose = { paxos_def::PX_PROPOSE };
				SocketReader().ReadRest<int8_t>(fd, propose);
				std::string data = SocketReader().ReadBlock(fd, propose.size);

				dataset.Propose(propose.zxId, std::move(data));

				paxos_def::ProposeAck ack = { paxos_def::PX_ACK_PROPOSE, propose.zxId };
				hn_send(fd, (const char*)&ack, sizeof(ack));
			}
			break;
		case paxos_def::PX_COMMIT: {
				paxos_def::Commit commit = { paxos_def::PX_COMMIT };
				SocketReader().ReadRest<int8_t>(fd, commit);

				dataset.Commit(commit.zxId);
			}
			break;
		case paxos_def::PX_PING: {
				type = paxos_def::PX_PONG;
				hn_send(fd, (const char *)&type, sizeof(type));
			}
			break;
		}
	}
}
