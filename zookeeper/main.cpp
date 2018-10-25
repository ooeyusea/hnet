#include "hnet.h"
#include "SimpleIni.h"
#include <stdio.h>
#include <string.h>
#include <vector>

class ZooKeeper {
	enum {
		LOOKING = 0,
		FOLLOW,
		LEADER,
	};

	struct Server {
		int32_t id;
		std::string ip;
		int32_t electionPort;
		int32_t votePort;

		int32_t electionFd = -1;
		int32_t voteFd = -1;
	};
public:
	ZooKeeper() {}
	~ZooKeeper() {}

	bool Start();
	void Run();

	void Election();
	void Leading();
	void Following();

private:
	int8_t _state = LOOKING;
	int32_t _id = 0;
	int32_t _clientPort = 0;
	std::string _ip;
	int32_t _electionPort = 0;
	int32_t _votePort = 0;

	std::vector<Server> _servers;

	int32_t _electionFd;
	int32_t _voteFd;
	int32_t _logicClock = 0;
	int32_t _zxId = 0;
	int32_t _voteId = 0;
	int32_t _voteZxId = 0;
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
		snprintf(key, 63, "server.", idx);

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

	_electionFd = hn_listen(_ip.c_str(), _electionFd);
	if (_electionFd < 0) {
		printf("zookeeper: listen election failed\n");
		return false;
	}

	return true;
}

void ZooKeeper::Run() {
	switch (_state) {
	case LOOKING: Election(); break;
	case FOLLOW: Following(); break;
	case LEADER: Leading(); break;
	}
}

void ZooKeeper::Election() {
	++_logicClock;
	_voteId = _id;
	_voteZxId = _zxId;

	
}

void start(int32_t argc, char ** argv) {
	ZooKeeper keeper;
	if (keeper.Start())
		keeper.Run();
}
