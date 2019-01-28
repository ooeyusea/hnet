#ifndef __LEAD_H__
#define __LEAD_H__
#include "hnet.h"
#include "util.h"
#include <string>
#include "DataSet.h"
#include "define.h"

class Lead {
	struct Message {
		int32_t id;
		std::string data;
	};
public:
	Lead();
	~Lead();

	int32_t Leading(int32_t id, int32_t peerEpoch, DataSet& dataset, int32_t votePort, int32_t serverCount);

private:
	void GetAcceptPeerEpoch(int32_t id, DataSet& dataset, int32_t serverCount, int32_t& peerEpoch);
	void WaitSynced(int32_t id, DataSet& dataset, int32_t serverCount, int32_t peerEpoch);
	void ProcessRequest(int32_t id, DataSet& dataset, int32_t serverCount, int32_t peerEpoch);

	bool StartListenFollow(int32_t id, int32_t serverCount, int32_t votePort);
	void ShutdownListen();

	void SendLeaderInfo(int32_t id, int32_t peerEpoch);
	void Diff(int32_t id, DataSet& dataset, int64_t lastZxId);
	void Trunc(int32_t id, DataSet& dataset, int64_t lastZxId);
	void Snap(int32_t id, DataSet& dataset);
	void Updated(int32_t id, DataSet& dataset);

	void DealRequest(int32_t id, const std::string& data);
	void DealAckPropose(int32_t id, int64_t zxId, DataSet& dataset);

private:
	hn_channel<Message, 100> _messages;
	int32_t _fd;

	std::vector<int32_t> _followers;
	std::atomic<int32_t> _count;
};

#endif //__LEAD_H__
