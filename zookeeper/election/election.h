#ifndef __ELECTION_H__
#define __ELECTION_H__
#include "hnet.h"
#include "util.h"
#include <string>
#include <unordered_map>

struct VoteHeader {
	int16_t size;
	int16_t id;

	enum {
		PING = 0,
		PONG,
		VOTE,
	};
};

struct Vote {
	int32_t idx;
	int8_t state;
	int32_t electionEpoch;
	int32_t voteId;
	int64_t voteZxId;

	bool operator<(const Vote& rhs) const;
};

class VoteSender {
public:
	VoteSender() {}
	~VoteSender() {}

	void Start(std::string ip, int32_t port);
	void Terminate() { _terminate = true; }
	void Send(const char * context, int32_t size);

private:
	void CheckTimeout();
	void DealPingpong();

private:
	int32_t _fd = -1;

	bool _terminate = false;
	int64_t _activeTick;
};

class VoteReciver {
public:
	VoteReciver() {}
	~VoteReciver() {}

	void Start(int32_t fd, hn_channel<Vote, -1> ch);

private:
	void CheckTimeout(int32_t fd);
	void TickSendPing(int32_t fd);
	void DealNetPacket(int32_t fd, hn_channel<Vote, -1> ch);

private:
	bool _terminate = false;
	int64_t _activeTick;
};

class Election {
public:
	enum {
		LOOKING = 0,
		FOLLOWING,
		LEADING,
	};

	Election() {}
	~Election() {}

	bool Start(int32_t clusterCount, const std::string& ip, int32_t electionPort, const std::vector<Server>& cluster);
	Vote LookForLeader(int32_t id, int64_t zxId, int32_t count);
	void Echo();

	bool IsVoteOk(const std::unordered_map<int32_t, Vote>& votes, int32_t leader, int32_t electionEpoch, int32_t count);
	bool CheckLeader(const std::unordered_map<int32_t, Vote>& votes, int32_t leader, bool IsEpochEqual);
	void BrocastVote(const Vote & vote);

	inline int32_t GetLogicCount() const { return _logicCount; }

private:
	hn_channel<Vote, -1> _recvCh;
	
	int32_t _id = -1;
	int8_t _state = LOOKING;
	int32_t _logicCount = 0;

	Vote _leader;
	hn_channel<int8_t, 1> _clearCh;
	bool _echoing = false;

	std::vector<VoteSender * > _senders;
};

#endif //__ELECTION_H__
