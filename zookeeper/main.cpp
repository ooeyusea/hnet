#include "hnet.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include "election.h"
#include "XmlReader.h"

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
	try {
		olib::XmlReader conf;
#ifdef WIN32
		if (conf.LoadXml("etc/zookeeper/conf.xml"))
#else
		if (conf.LoadXml("/etc/zookeeper/conf.xml"))
#endif
			return false;

		_id = conf.Root()["zookeeper"][0].GetAttributeInt32("id");
		if (_id == 0) {
			printf("zookeeper: invalid id\n");
			return false;
		}

		_clientPort = conf.Root()["zookeeper"][0].GetAttributeInt32("clientport");

		const auto& servers = conf.Root()["zookeeper"][0]["server"];
		for (int32_t i = 0; i < servers.Count(); ++i) {

			int32_t idx = conf.Root()["zookeeper"][0]["server"][i].GetAttributeInt32("id");
			std::string ip = conf.Root()["zookeeper"][0]["server"][i].GetAttributeString("ip");
			int32_t electionPort = conf.Root()["zookeeper"][0]["server"][i].GetAttributeInt32("election");
			int32_t votePort = conf.Root()["zookeeper"][0]["server"][i].GetAttributeInt32("vote");

			if (idx == _id) {
				_ip = ip;
				_electionPort = electionPort;
				_votePort = votePort;
			}
			else
				_servers.push_back({ idx, ip, electionPort, votePort });
		}
	}
	catch (std::exception& e) {
		printf("zookeeper load config failed : %s\n", e.what());
		return false;
	}

	if (_servers.size() % 2 != 0) {
		printf("zookeeper: server count must old\n");
		return false;
	}

	if (!_election.Start(_servers.size() + 1, _ip, _electionPort, _servers)) {
		printf("zookeeper: election start failed\n");
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
