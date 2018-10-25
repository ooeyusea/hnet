#ifndef __ELECTION_H__
#define __ELECTION_H__
#include "hnet.h"
#include <string>

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
	int32_t voteZxId;
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

	void Start(int32_t fd, hn_channel(Vote, -1) ch);

private:
	void CheckTimeout(int32_t fd);
	void TickSendPing(int32_t fd);
	void DealNetPacket(int32_t fd, hn_channel(Vote, -1) ch);

private:
	bool _terminate;
	int64_t _activeTick;
	int64_t _lastSendTick;
};

class Election {
public:
	Election() {}
	~Election() {}

	std::tuple<int8_t, int32_t> LookForLeader();

private:
	hn_channel(Vote, -1) _recvCh;
};

#endif //__ELECTION_H__
