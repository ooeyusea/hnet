#include "servernode.h"
#include "rpc.h"
#include "XmlReader.h"
#include "argument.h"
#include <set>
#include <vector>
#define RECONNECT_INVERVAL 1000

struct ClusterServer {
	int8_t type;
	int16_t id;
	std::string ip;
	int32_t port;
};

Cluster::Cluster() {
	Argument::Instance().RegArgument("id", 0, _id);
}

bool Cluster::Start() {
	int32_t listenPort = 0;
	std::vector<ClusterServer> connectServers;

	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

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

			if ((type != _service || id != _id) && connects.find(type) != connects.end()) {
				connectServers.push_back({ type, id, servers[i].GetAttributeString("ip"), servers[i].GetAttributeInt32("port") });
			}
			else if (type == _service && id == _id && servers[i].HasAttribute("port")) {
				listenPort = servers[i].GetAttributeInt32("port");
			}
		}

		if (conf.Root().IsExist("cluster_define") && conf.Root()["cluster_define"][0].HasAttribute("reconnect")) {
			_reconnectInterval = conf.Root()["cluster_define"][0].GetAttributeInt32("reconnect");
		}
		else
			_reconnectInterval = RECONNECT_INVERVAL;
	}
	catch (std::exception& e) {
		hn_error("Load Cluster Config : {}", e.what());
		return false;
	}

	if (listenPort > 0) {
		if (!ProvideService(listenPort)) {
			hn_error("Listen port {} failed", listenPort);
			return false;
		}

		hn_info("cluster listen port {}", listenPort);
	}

	for (auto& server : connectServers)
		RequestSevice(server.type, server.id, server.ip, server.port);

	hn_info("cluster start complte.");
	return true;
}

bool Cluster::ProvideService(int32_t port) {
	int32_t listenFd = hn_listen("0.0.0.0", port);
	if (!listenFd) {
		return false;
	}

	hn_fork [this, listenFd]() {
		while (!_terminate) {
			int32_t fd = hn_accept(listenFd);
			if (fd > 0) {
				hn_fork[fd, this]{
					char buff[sizeof(int32_t)];
					int32_t& serviceId = *(int32_t*)buff;

					int32_t pos = 0;
					while (pos < sizeof(int32_t)) {
						int32_t len = hn_recv(fd, buff + pos, sizeof(buff) - pos);
						if (len < 0)
							return;

						pos += len;
					}

					hn_info("service {} connected {}", serviceId, fd);

					_rpc.Attach(serviceId, fd);

					for (auto& f : _openListeners) {
						f((serviceId >> 16) & 0xFF, serviceId & 0xFFFF);
					}

					_rpc.Start(serviceId, fd);

					for (auto& f : _closeListeners) {
						f((serviceId >> 16) & 0xFF, serviceId & 0xFFFF);
					}

					hn_info("service {} disconnected {}", serviceId, fd);
				};
			}
		}
	};

	return true;
}

void Cluster::RequestSevice(int8_t service, int16_t id, const std::string& ip, int32_t port) {
	hn_fork [this, service, id, ip, port]{
		while (!_terminate) {
			int32_t fd = hn_connect(ip.c_str(), port);
			if (fd > 0) {
				hn_info("service {} connected {}", ServiceId(service, id), fd);

				int32_t serviceId = ServiceId(_service, _id);
				hn_send(fd, (const char *)&serviceId, sizeof(serviceId));

				_rpc.Attach(ServiceId(service, id), fd);

				for (auto& f : _openListeners) {
					f(service, id);
				}

				_rpc.Start(ServiceId(service, id), fd);

				for (auto& f : _closeListeners) {
					f(service, id);
				}

				hn_info("service {} disconnected {}", ServiceId(service, id), fd);
			}

			hn_sleep _reconnectInterval;
		}
	};
}
