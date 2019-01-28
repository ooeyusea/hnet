#include "election.h"
#include "util.h"

#define NET_BUFF_SIZE 1024
#define RECONNECT_INTERVAL 100
#define TIMEOUT_CHECK_INTERVAL 100
#define TIMEOUT 500
#define HEARTBET 200
#define WAIT_TIME 300
#define WAIT_CHECK_INTERVAL 50
#define ECHO_CEHCK_INTERVAL 100
#define TRY_POP_VOTE_INTERVAL 50
#define RESEND_VOTE_INTERVAL 200

bool Vote::operator<(const Vote& rhs) const {
	return (electionEpoch == rhs.electionEpoch) ? ((voteZxId == rhs.voteZxId ? (voteId < rhs.voteId) : voteZxId < rhs.voteZxId)) : electionEpoch < rhs.electionEpoch;
}

void VoteSender::Start(std::string ip, int32_t port) {
	while (!_terminate) {
		hn_debug("zookeeper: connect {}:{}\n", ip.c_str(), port);
		_fd = hn_connect(ip.c_str(), port);
		if (_fd < 0) {
			//log
			hn_sleep RECONNECT_INTERVAL;
			continue;
		}
		_activeTick = zookeeper::GetTickCount();

		auto test = DoWork([this] { CheckTimeout(); });
		auto pong = DoWork([this] { DealPingpong(); });

		test.Wait();
		hn_close(_fd);
		_fd = -1;

		pong.Wait();
	}
}

void VoteSender::CheckTimeout() {
	while (!_terminate) {
		hn_sleep TIMEOUT_CHECK_INTERVAL;

		int64_t now = zookeeper::GetTickCount();
		if (now - _activeTick > TIMEOUT)
			break;
	}
}

void VoteSender::DealPingpong() {
	char buff[NET_BUFF_SIZE];
	int32_t offset = 0;
	while (true) {
		int32_t len = hn_recv(_fd, buff + offset, NET_BUFF_SIZE - offset);
		if (len < 0)
			break;

		offset += len;

		int32_t pos = 0;
		while (offset - pos >= sizeof(VoteHeader)) {
			VoteHeader& header = *(VoteHeader*)(buff + pos);
			if (offset - pos < header.size)
				break;

			if (header.id == VoteHeader::PING) {
				VoteHeader pong;
				pong.size = sizeof(VoteHeader);
				pong.id = VoteHeader::PONG;

				hn_send(_fd, (const char*)&pong, sizeof(pong));
				_activeTick = zookeeper::GetTickCount();
			}
			else {
				//assert(false);
			}

			pos += header.size;
		}

		if (pos < offset) {
			memmove(buff, buff + pos, offset - pos);
			offset -= pos;
		}
		else
			offset = 0;
	}
}

void VoteSender::Send(const char * context, int32_t size) {
	if (_fd < 0)
		return;
	
	hn_send(_fd, context, size);
}

void VoteReciver::Start(int32_t fd, hn_channel<Vote, -1> ch) {
	_activeTick = zookeeper::GetTickCount();

	auto test = DoWork([this, fd] { CheckTimeout(fd); });
	auto tsp = DoWork([this, fd] { TickSendPing(fd); });
	auto np = DoWork([this, fd, ch] { DealNetPacket(fd, ch); });

	np.Wait();

	_terminate = true;
	test.Wait();
	tsp.Wait();
}

void VoteReciver::CheckTimeout(int32_t fd) {
	while (!_terminate) {
		hn_sleep TIMEOUT_CHECK_INTERVAL;

		int64_t now = zookeeper::GetTickCount();
		if (now - _activeTick > TIMEOUT)
			break;
	}

	hn_close(fd);
}

void VoteReciver::TickSendPing(int32_t fd) {
	while (!_terminate) {
		hn_sleep HEARTBET;

		VoteHeader ping;
		ping.size = sizeof(VoteHeader);
		ping.id = VoteHeader::PING;

		hn_send(fd, (const char*)&ping, sizeof(ping));
	}
}

void VoteReciver::DealNetPacket(int32_t fd, hn_channel<Vote, -1> ch) {
	while (true) {
		char buff[NET_BUFF_SIZE];
		int32_t offset = 0;
		while (true) {
			int32_t len = hn_recv(fd, buff + offset, NET_BUFF_SIZE - offset);
			if (len < 0)
				break;
			offset += len;

			int32_t pos = 0;
			while (offset - pos >= sizeof(VoteHeader)) {
				VoteHeader& header = *(VoteHeader*)(buff + pos);
				if (offset - pos < header.size)
					break;

				if (header.id == VoteHeader::PONG)
					_activeTick = zookeeper::GetTickCount();
				else if (header.id == VoteHeader::VOTE) {
					//hn_debug("zookeeper:recv vote\n");
					Vote& vote = *(Vote*)(buff + pos + sizeof(VoteHeader));
					try {
						ch << vote;
					}
					catch (hn_channel_close_exception& e) {

					}
				}
				else {
					//assert(false);
				}
				pos += header.size;
			}

			if (pos < offset) {
				memmove(buff, buff + pos, offset - pos);
				offset -= pos;
			}
			else
				offset = 0;
		}
	}
}

bool Election::Start(int32_t clusterCount, const std::string& ip, int32_t electionPort, const std::vector<Server>& cluster) {
	int32_t fd = hn_listen(ip.c_str(), electionPort);
	if (fd < 0) {
		hn_error("election start failed\n");
		return false;
	}
	
	_senders.resize(clusterCount, nullptr);

	hn_fork [this, fd]{
		while (true) {
			int32_t remote = hn_accept(fd);
			if (remote < 0)
				break;
			
			hn_fork [this, remote]{
				VoteReciver reciver;
				reciver.Start(remote, _recvCh);
			};
		}

		hn_info("listen election port failed\n");
	};

	for (auto& server : cluster) {
		hn_fork [this, server]{
			VoteSender sender;
			_senders[server.id - 1] = &sender;

			sender.Start(server.ip, server.electionPort);
		};
	}
	return true;
}

Vote Election::LookForLeader(int32_t id, int64_t zxId, int32_t count) {
	hn_info("start looking for leader\n");
	_id = id;
	++_logicCount;
	if (_echoing) {
		_echoing = false;

		int8_t res;
		_clearCh >> res;
	}

	_state = LOOKING;

	std::unordered_map<int32_t, Vote> votes;
	Vote proposal{id, _state, _logicCount, id, zxId};

	std::unordered_map<int32_t, Vote> otherVote;

	votes[id] = proposal;

	int64_t mark = zookeeper::GetTickCount();
	std::list<Vote> queue;
	while (true) {
		Vote vote;
		if (!queue.empty()) {
			vote = queue.front();
			queue.pop_front();
		}
		else if (queue.empty() && !_recvCh.TryPop(vote)) {
			hn_sleep TRY_POP_VOTE_INTERVAL;

			int64_t now = zookeeper::GetTickCount();
			if (now - mark >= RESEND_VOTE_INTERVAL) {
				mark = now;

				BrocastVote(proposal);
			}
			continue;
		}

		switch (vote.state) {
		case LOOKING:
			if (vote.electionEpoch > _logicCount) {
				_logicCount = vote.electionEpoch;
				votes.clear();

				votes[vote.idx] = vote;

				proposal.electionEpoch = vote.electionEpoch;
				if (proposal < vote) {
					proposal.voteId = vote.voteId;
					proposal.voteZxId = vote.voteZxId;
					votes[id] = proposal;

					hn_debug("zookeeper: 1 other vote is bigger {} {} {}\n", vote.voteId, vote.electionEpoch, vote.voteZxId);
				}

				BrocastVote(proposal);
			}
			else if (vote.electionEpoch < _logicCount) {
				break;
			}
			else {
				votes[vote.idx] = vote;

				if (proposal < vote) {
					proposal.voteId = vote.voteId;
					proposal.voteZxId = vote.voteZxId;
					votes[id] = proposal;

					BrocastVote(proposal);

					hn_debug("zookeeper: 2 other vote is bigger {} {} {}\n", vote.voteId, vote.electionEpoch, vote.voteZxId);
				}
			}

			if (IsVoteOk(votes, vote.voteId, vote.electionEpoch, count)) {
				int64_t check = zookeeper::GetTickCount();
				while (true) {
					if (zookeeper::GetTickCount() - check > WAIT_TIME)
						break;

					hn_sleep WAIT_CHECK_INTERVAL;

					Vote n;
					if (!_recvCh.TryPop(vote))
						continue;

					if (proposal < vote) {
						queue.push_back(n);
						break;
					}
				}

				if (queue.empty()) {
					_state = (proposal.voteId == _id) ? LEADING : FOLLOWING;
					_leader = proposal;
					hn_debug("zookeeper: leader is {} {} {}\n", _leader.voteId, _leader.electionEpoch, _leader.voteZxId);
					Echo();
					return proposal;
				}
			}
			break;
		case FOLLOWING:
		case LEADING:
			if (vote.electionEpoch == _logicCount) {
				votes[vote.idx] = vote;

				if (IsVoteOk(votes, vote.voteId, vote.electionEpoch, count) && CheckLeader(otherVote, vote.voteId, true)) {
					_state = (vote.voteId == _id) ? LEADING : FOLLOWING;
					_leader = vote;
					Echo();
					return vote;
				}
			}

			otherVote[vote.idx] = { vote.idx, vote.state, -1, vote.voteId, -1 };
			if (IsVoteOk(otherVote, vote.voteId, -1, count) && CheckLeader(otherVote, vote.voteId, false)) {
				_state = (vote.voteId == _id) ? LEADING : FOLLOWING;
				_leader = vote;
				Echo();
				return vote;
			}
			break;
		}
	}
}

void Election::Echo() {
	_echoing = true;
	hn_fork [this]{
		_echoing = true;
		while (_echoing) {
			Vote vote;
			if (!_recvCh.TryPop(vote)) {
				hn_sleep ECHO_CEHCK_INTERVAL;
				continue;
			}

			if (vote.state == LOOKING) {
				char msg[sizeof(VoteHeader) + sizeof(Vote)];
				VoteHeader& header = *(VoteHeader*)msg;
				Vote& send = *(Vote*)(msg + sizeof(VoteHeader));
				header.id = VoteHeader::VOTE;
				header.size = sizeof(msg);
				send = _leader;
				send.state = _state;

				if (vote.idx >= 1 && vote.idx <= (int32_t)_senders.size())
					_senders[vote.idx-1]->Send(msg, header.size);
			}
		}
		_clearCh << (int8_t)1;
	};
}

bool Election::IsVoteOk(const std::unordered_map<int32_t, Vote>& votes, int32_t leader, int32_t electionEpoch, int32_t count) {
	int32_t vote = 0;
	for (auto p : votes) {
		if (p.second.electionEpoch == electionEpoch && p.second.voteId == leader)
			++vote;
	}

	return (vote >= (count / 2 + 1));
}

bool Election::CheckLeader(const std::unordered_map<int32_t, Vote>& votes, int32_t leader, bool IsEpochEqual) {
	if (leader == _id) 
		return IsEpochEqual;
	
	auto itr = votes.find(leader);
	if (itr != votes.end() && itr->second.state == LEADING)
		return true;

	return false;
}

void Election::BrocastVote(const Vote & vote) {
	//hn_debug("zookeeper:brocast vote\n");
	char msg[sizeof(VoteHeader) + sizeof(Vote)];
	VoteHeader& header = *(VoteHeader*)msg;
	Vote& send = *(Vote*)(msg + sizeof(VoteHeader));
	header.id = VoteHeader::VOTE;
	header.size = sizeof(msg);
	send = vote;
	send.state = _state;

	for (auto * sender : _senders) {
		if (sender)
			sender->Send(msg, sizeof(msg));
	}
}

