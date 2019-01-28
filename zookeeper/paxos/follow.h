#ifndef __FOLLOW_H__
#define __FOLLOW_H__
#include "hnet.h"
#include "util.h"
#include <string>
#include "DataSet.h"

class Follow {
public:
	Follow();
	~Follow();

	int32_t Following(int32_t id, int32_t peerEpoch, DataSet& dataset, const std::string& leaderIp, int32_t leaderPort);

private:
	void RegisterToLeader(int32_t fd, int32_t id, DataSet& dataset, int32_t& peerEpoch);
	void SyncWithLeader(int32_t fd, int32_t id, DataSet& dataset);
	void ProcossPropose(int32_t fd, int32_t id, DataSet& dataset);

private:

};

#endif //__FOLLOW_H__
