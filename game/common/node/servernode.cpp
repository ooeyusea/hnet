#include "servernode.h"
#include "rpc.h"
#include "XmlReader.h"
#include "argument.h"
#include <set>
#include <vector>

struct ClusterServer {
	int8_t type;
	int16_t id;
	std::string ip;
	int32_t port;
};

Cluster::Cluster() {
	Argument::Instance().RegArgument("type", 0, _service);
	Argument::Instance().RegArgument("node", 0, _id);
}

bool Cluster::Start() {
	olib::XmlReader conf;
	if (!conf.LoadXml("conf.xml")) {
		return false;
	}

	int32_t listenPort = 0;
	std::vector<ClusterServer> connectServers;

	try {
		std::set<int8_t> connects;

		auto& clusters = conf.Root()["cluster"];
		for (int32_t i = 0; i < clusters.Count(); ++i) {
			if (clusters[i].GetAttributeInt8("type") == _service) {
				auto& connect = clusters[i]["connect"];
				for (int32_t j = 0; j < connect.Count(); ++j) {
					connects.insert(connect[j].GetAttributeInt8("type"));
				}
			}
		}

		auto& servers = conf.Root()["server"];
		for (int32_t i = 0; i < servers.Count(); ++i) {
			int8_t type = servers[i].GetAttributeInt8("type");
			int16_t id = servers[i].GetAttributeInt16("id");

			if (type != _service && id != _id && connects.find(type) != connects.end()) {
				connectServers.push_back({ type, id, servers[i].GetAttributeString("ip"), servers[i].GetAttributeInt32("port") });
			}
			else if (type == _service && id == _id && servers[i].HasAttribute("port")) {
				listenPort = servers[i].GetAttributeInt32("port");
			}
		}
	}
	catch (std::exception& e) {
		return false;
	}

	if (listenPort > 0)
		ProvideService(listenPort);

	for (auto& server : connectServers)
		RequestSevice(server.type, server.id, server.ip, server.port);

	return true;
}

void Cluster::RegisterServiceOpen(const std::function<void(int8_t service, int16_t id)>& f) {

}

void Cluster::RegisterServiceClose(const std::function<void(int8_t service, int16_t id)>& f) {

}

void Cluster::ProvideService(int32_t port) {

}

void Cluster::RequestSevice(int8_t service, int16_t id, const std::string& ip, int32_t port) {

}
