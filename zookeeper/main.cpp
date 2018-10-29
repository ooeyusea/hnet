#include "hnet.h"
#include "SimpleIni.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include "election.h"

class ZooKeeper {
public:
	ZooKeeper() {}
	~ZooKeeper() {}

	bool Start();
	void Run();

	void Elect();
	void Leading();
	void Following();

private:
	int8_t _state = Election::LOOKING;
	int32_t _id = 0;
	int32_t _zxId = 0;
	int32_t _clientPort = 0;
	std::string _ip;
	int32_t _electionPort = 0;
	int32_t _votePort = 0;

	std::vector<Server> _servers;

	Election _election;
};

bool ZooKeeper::Start() {
	CSimpleIniA ini(true, true, true);
	ini.Reset();
	if (ini.LoadFile("zookeeper.ini") < 0)
		return false;

	_id = ini.GetLongValue("zookeeper", "id");
	if (_id == 0) {
		printf("zookeeper: invalid id\n");
		return false;
	}

	_clientPort = ini.GetLongValue("zookeeper", "clientport");

	int32_t idx = 0;
	while (true) {
		++idx;

		char key[64] = { 0 };
		snprintf(key, 63, "server.%d", idx);

		const char * value = ini.GetValue("zookeeper", key);
		if (!value)
			break;

		const char * pos = strstr(value, ":");
		if (!pos) {
			printf("zookeeper: invalid server config\n");
			return false;
		}

		const char * pos1 = strstr(pos + 1, ":");
		if (!pos) {
			printf("zookeeper: invalid server config\n");
			return false;
		}

		std::string ip(value, pos);
		int32_t electionPort = atoi(std::string(pos + 1, pos1 - pos - 1).c_str());
		int32_t votePort = atoi(std::string(pos1 + 1).c_str());

		if (idx == _id) {
			_ip = ip;
			_electionPort = electionPort;
			_votePort = votePort;
		}
		else
			_servers.push_back({ idx, ip, electionPort, votePort });
	}

	if (idx % 2 != 0) {
		printf("zookeeper: server count must old\n");
		return false;
	}

	if (!_election.Start(_servers.size() + 1, _ip, _electionPort, _servers)) {
		printf("zookeeper: election start failed");
		return false;
	}

	return true;
}

void ZooKeeper::Run() {
	while (true) {
		switch (_state) {
		case Election::LOOKING: Elect(); break;
		case Election::FOLLOWING: Following(); break;
		case Election::LEADING: Leading(); break;
		}
	}
}

void ZooKeeper::Elect() {
	Vote vote = _election.LookForLeader(_id, _zxId, _servers.size() + 1);

	_state = (vote.voteId == _id) ? Election::LEADING : Election::FOLLOWING;
}

void ZooKeeper::Leading() {
	printf("I'm leader\n");
	hn_sleep(5000);
}

void ZooKeeper::Following() {
	printf("I'm follow\n");
	hn_sleep(5000);
}

void start(int32_t argc, char ** argv) {
	ZooKeeper keeper;
	if (keeper.Start())
		keeper.Run();
}
