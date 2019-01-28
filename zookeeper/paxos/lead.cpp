#include "lead.h"
#include "util.h"
#include "socket_helper.h"

Lead::Lead() {
}

Lead::~Lead() {

}

int32_t Lead::Leading(int32_t id, int32_t peerEpoch, DataSet& dataset, int32_t votePort, int32_t serverCount) {
	if (!StartListenFollow(id, serverCount, votePort))
		return peerEpoch;

	try {
		GetAcceptPeerEpoch(id, dataset, serverCount, peerEpoch);

		WaitSynced(id, dataset, serverCount, peerEpoch);

		ProcessRequest(id, dataset, serverCount, peerEpoch);

		dataset.Flush();
	}
	catch (hn_channel_close_exception& e) {
		hn_error("half follower close");
	}
	catch (std::exception& e) {
		hn_error("leading exception {}", e.what());
	}

	ShutdownListen();

	return peerEpoch;
}

void Lead::GetAcceptPeerEpoch(int32_t id, DataSet& dataset, int32_t serverCount, int32_t& peerEpoch) {
	std::vector<int32_t> recvPeerEpoch;
	recvPeerEpoch.resize(serverCount, 0);
	recvPeerEpoch[id - 1] = peerEpoch;

	while (true) {
		Message msg;
		_messages >> msg;
		
		if (*(int8_t*)msg.data.data() != paxos_def::PX_FOLLOW_INFO) {
			throw std::logic_error("recieve unexpect message");
		}

		paxos_def::FollowInfo& followInfo = *(paxos_def::FollowInfo*)msg.data.data();
		recvPeerEpoch[msg.id - 1] = followInfo.peerEpoch;

		int32_t count = 0;
		for (auto epoch : recvPeerEpoch) {
			if (epoch > 0) {
				++count;
			}
		}

		if (count >= serverCount / 2 + 1) {
			int64_t tick = zookeeper::GetTickCount();
			while (true) {
				if (_messages.TryPop(msg)) {
					if (*(int8_t*)msg.data.data() != paxos_def::PX_FOLLOW_INFO) {
						throw std::logic_error("recieve unexpect message");
					}

					paxos_def::FollowInfo& followInfo = *(paxos_def::FollowInfo*)msg.data.data();
					recvPeerEpoch[msg.id - 1] = followInfo.peerEpoch;
				}
				else {
					hn_sleep 20;
				}

				if (zookeeper::GetTickCount() >= tick + 500) {
					break;
				}
			}

			for (auto epoch : recvPeerEpoch) {
				if (epoch > 0 && epoch > peerEpoch)
					peerEpoch = epoch;
			}

			break;
		}
	}

	peerEpoch = peerEpoch + 1;
	for (int32_t i = 0; i < serverCount; ++i) {
		if (i != id - 1 && recvPeerEpoch[i] > 0)
			SendLeaderInfo(i + 1, peerEpoch);
	}
}

void Lead::WaitSynced(int32_t id, DataSet& dataset, int32_t serverCount, int32_t peerEpoch) {
	std::vector<bool> synced;
	synced.resize(serverCount, false);
	synced[id - 1] = true;

	while (true) {
		Message msg;
		_messages >> msg;

		switch (*(int8_t*)msg.data.data()) {
		case paxos_def::PX_FOLLOW_INFO: SendLeaderInfo(msg.id, peerEpoch); break;
		case paxos_def::PX_ACK_EPOCH: {
				paxos_def::AckEpoch& ackEpoch = *(paxos_def::AckEpoch*)msg.data.data();
				if (ackEpoch.lastZxId < dataset.GetMinZxId()) {
					Snap(msg.id, dataset);
				}
				else if (ackEpoch.lastZxId <= dataset.GetZxId()) {
					Diff(msg.id, dataset, ackEpoch.lastZxId);
				}
				else if (ackEpoch.lastZxId > dataset.GetZxId()) {
					Trunc(msg.id, dataset, ackEpoch.lastZxId);
				}
			}
			break;
		case paxos_def::PX_ACK: {
				Updated(msg.id, dataset);
				synced[msg.id - 1] = true;
			}
			break;
		}

		int32_t count = 0;
		for (auto sync : synced) {
			if (sync)
				++count;
		}

		if (count >= serverCount / 2 + 1)
			break;
	}
}

void Lead::ProcessRequest(int32_t id, DataSet& dataset, int32_t serverCount, int32_t peerEpoch) {
	while (true) {
		Message msg;
		_messages >> msg;

		switch (*(int8_t*)msg.data.data()) {
		case paxos_def::PX_FOLLOW_INFO: SendLeaderInfo(msg.id, peerEpoch); break;
		case paxos_def::PX_ACK_EPOCH: {
				paxos_def::AckEpoch& ackEpoch = *(paxos_def::AckEpoch*)msg.data.data();
				if (ackEpoch.lastZxId < dataset.GetMinZxId()) {
					Snap(msg.id, dataset);
				}
				else if (ackEpoch.lastZxId <= dataset.GetZxId()) {
					Diff(msg.id, dataset, ackEpoch.lastZxId);
				}
				else if (ackEpoch.lastZxId > dataset.GetZxId()) {
					Trunc(msg.id, dataset, ackEpoch.lastZxId);
				}
			}
			break;
		case paxos_def::PX_ACK: {
				Updated(msg.id, dataset);
			}
			break;
		case paxos_def::PX_REQUEST: {
				DealRequest(msg.id, msg.data);
			}
			break;
		case paxos_def::PX_ACK_PROPOSE: {
				paxos_def::ProposeAck& ackPropose = *(paxos_def::ProposeAck*)msg.data.data();
				DealAckPropose(msg.id, ackPropose.zxId, dataset);
			}
			break;
		}
	}
}

bool Lead::StartListenFollow(int32_t id, int32_t serverCount, int32_t votePort) {
	_followers.resize(serverCount, 0);

	_fd = hn_listen("0.0.0.0", votePort);
	if (_fd < 0)
		return false;

	++_count;
	hn_fork [this] {
		while (true) {
			int32_t remoteFd = hn_accept(_fd);
			if (remoteFd < 0)
				break;

			++_count;
			hn_fork [this, remoteFd] {
				int32_t id = 0;
				try {
					SocketReader().ReadType(remoteFd, id);
					_followers[id - 1] = remoteFd;

					while (true) {
						int8_t type;
						SocketReader().ReadType(remoteFd, type);

						Message msg;
						msg.id = id;

						switch (type) {
						case paxos_def::PX_FOLLOW_INFO: {
								paxos_def::FollowInfo info = { paxos_def::PX_FOLLOW_INFO };
								SocketReader().ReadRest<int8_t>(remoteFd, info);
								msg.data.assign((const char*)&info, sizeof(info));

								_messages << msg;
							}
							break;
						case paxos_def::PX_ACK_EPOCH: {
								paxos_def::AckEpoch info = { paxos_def::PX_ACK_EPOCH };
								SocketReader().ReadRest<int8_t>(remoteFd, info);
								msg.data.assign((const char*)&info, sizeof(info));

								_messages << msg;
							}
							break;
						case paxos_def::PX_ACK: {
								msg.data.assign((const char*)&type, sizeof(type));

								_messages << msg;
							}
							break;
						case paxos_def::PX_PONG: break;
						case paxos_def::PX_REQUEST: {
								paxos_def::Request info = { paxos_def::PX_REQUEST };
								SocketReader().ReadRest<int8_t>(remoteFd, info);
								msg.data.assign((const char*)&info, sizeof(info));

								_messages << msg;
							}
							break;
						case paxos_def::PX_ACK_PROPOSE:{
								paxos_def::ProposeAck info = { paxos_def::PX_ACK_PROPOSE };
								SocketReader().ReadRest<int8_t>(remoteFd, info);
								msg.data.assign((const char*)&info, sizeof(info));

								_messages << msg;
							}
							break;
						}
					}
				}
				catch (hn_channel_close_exception& e) {
					hn_info("message channel close {}", id);
				}
				catch (std::exception & e) {
					hn_info("follower close {}", id);
				}

				if (id > 0)
					_followers[id - 1] = 0;

				hn_shutdown(remoteFd);
				--_count;
			};
		}
		--_count;
	};

	return true;
}

void Lead::ShutdownListen() {
	hn_shutdown(_fd);
	_messages.Close();

	while (_count > 0) {
		hn_sleep 100;
	}
}

void Lead::SendLeaderInfo(int32_t id, int32_t peerEpoch) {

}

void Lead::Diff(int32_t id, DataSet& dataset, int64_t lastZxId) {

}

void Lead::Trunc(int32_t id, DataSet& dataset, int64_t lastZxId) {

}

void Lead::Snap(int32_t id, DataSet& dataset) {

}

void Lead::Updated(int32_t id, DataSet& dataset) {

}


void Lead::DealRequest(int32_t id, const std::string& data) {

}

void Lead::DealAckPropose(int32_t id, int64_t zxId, DataSet& dataset) {

}
