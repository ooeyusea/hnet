#include "hnet.h"
#include <string>
#include <vector>
#include <WS2tcpip.h>
#include <map>
#include <string.h>

class DnsParse {
	struct ipHost {
		const std::string& domain;
		std::string ip;
	};

public:
	static DnsParse& Instance() {
		static DnsParse g_instance;
		return g_instance;
	}

	static void ThreadProc(void * data) {
		ipHost& host = *(ipHost*)data;

		struct addrinfo hint;
		memset(&hint, 0, sizeof(hint));
		hint.ai_family = AF_INET;
		hint.ai_socktype = SOCK_STREAM;

		struct addrinfo * result = nullptr;
		if (getaddrinfo(host.domain.c_str(), nullptr, &hint, &result) != 0)
			return;

		char ip[64] = { 0 };
		inet_ntop(result->ai_family, &(((struct sockaddr_in *)(result->ai_addr))->sin_addr), ip, sizeof(ip) - 1);
		freeaddrinfo(result);

		host.ip = ip;
	}

	std::string GetIpFromhost(const std::string& host) {
		ipHost res{ host };
		_queue->Call(rand() % 4, &res);

		return std::move(res.ip);
	}

private:
	DnsParse() {
		_queue = hn_create_async(4, false, ThreadProc);
	}

	~DnsParse() {
		_queue->Shutdown();
	}

private:
	hyper_net::IAsyncQueue * _queue;
};

inline std::tuple<std::string, std::string, int32_t> ParseHost(const std::string& host) {
	int32_t port = 80;
	std::string hostWithoutPort;
	std::string::size_type pos = host.find(":");
	if (pos != std::string::npos) {
		hostWithoutPort = host.substr(0, pos);
		port = atoi(host.substr(pos + 1).c_str());
	}
	else
		hostWithoutPort = host;

	return std::make_tuple(hostWithoutPort, DnsParse::Instance().GetIpFromhost(hostWithoutPort), port);
}

inline std::tuple<std::string, std::string, int32_t, std::string> ParseURI(const std::string& uri) {
	std::string host;
	std::string get;

	std::string::size_type pos = uri.find("/");
	if (pos != std::string::npos) {
		host = uri.substr(0, pos);
		get = uri.substr(pos);
	}
	else {
		host = uri;
		get = "/";
	}

	pos = host.find("://");
	if (pos != std::string::npos)
		host = host.substr(pos + 3);

	auto hostAndPort = ParseHost(host);

	return std::make_tuple(std::get<0>(hostAndPort), std::get<1>(hostAndPort), std::get<2>(hostAndPort), get);
}

inline std::string MakeRequest(const std::string &host, const std::string &get)
{
	std::string http
		= "GET " + get + " HTTP/1.1\r\n"
		+ "HOST: " + host + "\r\n"
		+ "Connection: close\r\n\r\n";
	return std::move(http);
}

void GetContent(hn_channel(std::string, 20)& ch, const std::string& uri) {
	hn_fork [&ch, &uri]{
		auto t = ParseURI(uri);
		auto s = hn_connect(std::get<1>(t).c_str(), std::get<2>(t));
		if (s) {
			std::string request = MakeRequest(std::get<0>(t), std::get<3>(t));
			hn_send(s, request.data(), request.size());

			std::string res;
			char msg[1024] = { 0 };
			int32_t size = hn_recv(s, msg, 1023);
			while (size > 0) {
				res.append(msg, size);

				size = hn_recv(s, msg, 1023);
			}
			hn_close(s);

			std::string::size_type pos = res.find("\r\n");
			if (pos == std::string::npos) {
				res == "";
				ch << res;
				return;
			}

			std::string first = res.substr(0, pos);
			res = res.substr(pos + 2);

			if (first.find(" 200 ") == std::string::npos) {
				res == "";
				ch << res;
				return;
			}

			pos = res.find("\r\n");
			while (pos != std::string::npos && pos != 0) {
				res = res.substr(pos + 2);
				pos = res.find("\r\n");
			}

			if (pos == std::string::npos) {
				res == "";
				ch << res;
				return;
			}

			res = res.substr(pos + 2);

			ch << res;
		}
	};
}

void start(int32_t argc, char ** argv) {
	std::string uri = argv[1];
	int32_t count = atoi(argv[2]);

	//hn_channel(std::string, 20) uri;
	hn_channel(std::string, 20) result;
	
	GetContent(result, uri);

	std::string res;
	result >> res;
	printf("%s\n", res.c_str());
}
