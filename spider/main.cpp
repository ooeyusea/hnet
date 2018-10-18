#include "hnet.h"
#include <string>
#include <vector>
#include <WS2tcpip.h>
#include <map>
#include <string.h>
#include <set>
#include <io.h>
#include <direct.h>
#define PATH_DELIMITER '/'

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

	std::string::size_type st = uri.find("://");
	if (st != std::string::npos)
		st += 3;
	else
		st = 0;

	std::string::size_type pos = uri.find("/", st);
	if (pos != std::string::npos) {
		host = uri.substr(st, pos - st);
		get = uri.substr(pos);
	}
	else {
		host = uri.substr(st);
		get = "/";
	}

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

struct UriOp {
	std::string uri;
	bool add = true;
};

inline void ParseContent(hn_channel(UriOp, -1)& ch, const std::string& uri, const std::string& content) {
	std::string::size_type index;
	std::string::size_type pos = content.find("<a");
	while (pos != std::string::npos) {
		std::string::size_type hrefPos = content.find("href", pos);
		std::string::size_type left = content.find("\"", hrefPos);
		std::string::size_type right = content.find("\"", left + 1);

		std::string targetUri = content.substr(left + 1, right - left - 1);
		if (targetUri != "" && targetUri.find("JavaScript") == std::string::npos && targetUri.find("javascript") == std::string::npos) {
			if (targetUri.find("http") == std::string::npos) {
				if (targetUri.at(0) != '#') {
					std::string::size_type bg = uri.find("://");
					if (bg != std::string::npos)
						bg += 3;
					else
						bg = 0;

					std::string::size_type pos = uri.find("/", bg);
					if (targetUri.at(0) != '/') {
						if (pos != std::string::npos)
							targetUri = uri.substr(0, pos) + "/" + targetUri;
						else
							targetUri = uri + "/" + targetUri;
					}
					else {
						if (pos != std::string::npos)
							targetUri = uri.substr(0, pos) + targetUri;
						else
							targetUri = uri + targetUri;
					}
				}

				UriOp op{ targetUri, true };
				ch << op;
			}
		}

		pos = content.find("<a", pos + 2);
	}
}

inline std::string CreateReDirectory(const std::string& folder) {
	std::string folder_builder = "res\\";
	std::string sub;
	folder_builder.reserve(folder.size());

	if (0 != ::_access(folder_builder.c_str(), 0)) {
		// this folder not exist
		if (0 != ::_mkdir(folder_builder.c_str())) {
			return "";
		}
	}

	auto bg = folder.begin();
	auto pos = folder.find("//");
	if (pos != std::string::npos)
		bg += pos + 2;

	for (auto it = bg; it != folder.end(); ++it) {
		const char c = *it;
		if (c == PATH_DELIMITER) {
			sub.push_back('\\');
			folder_builder.append(sub);
			if (0 != ::_access(folder_builder.c_str(), 0)) {
				// this folder not exist
				if (0 != ::_mkdir(folder_builder.c_str())) {
					return "";
				}
			}
			sub.clear();
		}
		else
			sub.push_back(c);
	}
	if (sub == "")
		folder_builder.append("[index]");
	else {
		if (sub.at(0) == '?') {
			folder_builder.append("[+]");
			folder_builder.append(sub.substr(1));
		}
		else
			folder_builder.append(sub);
	}
	return std::move(folder_builder);
}

inline void WriteToFile(const std::string& path, const std::string& content) {
	FILE * fp = fopen(path.c_str(), "w");
	if (fp) {
		fwrite(content.data(), content.size(), 1, fp);
		fflush(fp);
		fclose(fp);
	}
}

void ReadHtml(hn_channel(std::string, 20)& uriCh, hn_channel(UriOp, -1)& ch) {
	hn_fork hn_stack(10 * 1024 * 1024) [&uriCh, &ch]{
		try {
			while (true) {
				std::string uri;
				uriCh >> uri;

				printf("process %s\n", uri.c_str());

				UriOp res;
				res.uri = uri;
				res.add = false;

				auto t = ParseURI(uri);
				auto s = hn_connect(std::get<1>(t).c_str(), std::get<2>(t));
				if (s > 0) {
					std::string request = MakeRequest(std::get<0>(t), std::get<3>(t));
					hn_send(s, request.data(), request.size());

					std::string content;
					char msg[1024] = { 0 };
					int32_t size = hn_recv(s, msg, 1023);
					while (size > 0) {
						content.append(msg, size);

						size = hn_recv(s, msg, 1023);
					}
					hn_close(s);

					std::string::size_type pos = content.find("\r\n");
					if (pos == std::string::npos) {
						content == "";
						ch << res;
						return;
					}

					std::string first = content.substr(0, pos);
					content = content.substr(pos + 2);

					if (first.find(" 200 ") == std::string::npos) {
						content == "";
						ch << res;
						return;
					}

					pos = content.find("\r\n");
					while (pos != std::string::npos && pos != 0) {
						content = content.substr(pos + 2);
						pos = content.find("\r\n");
					}

					if (pos == std::string::npos) {
						content == "";
						ch << res;
						return;
					}

					content = content.substr(pos + 2);

					ParseContent(ch, uri, content);

					std::string filePath = CreateReDirectory(uri);
					if (filePath != "")
						WriteToFile(filePath, content);
				}
				ch << res;
			}
		}
		catch (hn_channel_close_exception& e) {

		}
	};
}

inline std::string ParseDomain(const std::string& uri) {
	std::string host;

	std::string::size_type st = uri.find("://");
	if (st != std::string::npos)
		st += 3;
	else
		st = 0;

	std::string::size_type pos = uri.find("/", st);
	if (pos != std::string::npos) {
		host = uri.substr(st, pos - st);
	}
	else {
		host = uri.substr(st);
	}

	pos = host.rfind(".");
	if (pos != std::string::npos) {
		pos = host.rfind(".", pos + 1);
	}
	
	if (pos != std::string::npos)
		host = host.substr(host.size() - pos);

	return std::move(host);
}

void start(int32_t argc, char ** argv) {
	std::string uri = argv[1];
	int32_t count = atoi(argv[2]);
	int32_t threadCount = atoi(argv[3]);

	hn_channel(std::string, 20) uriCh;
	hn_channel(UriOp, -1) opCh;

	for (int32_t i = 0; i < threadCount; ++i)
		ReadHtml(uriCh, opCh);

	std::set<std::string> processUris;
	std::set<std::string> processedUris;

	processUris.insert(uri);
	processedUris.insert(uri);
	uriCh << uri;

	std::string domain = ParseDomain(uri);
	while (!processUris.empty()) {
		UriOp op;
		opCh >> op;

		std::string opDomain = ParseDomain(op.uri);
		if (op.add && count != 0 && processedUris.find(op.uri) == processedUris.end() && opDomain == domain) {
			processUris.insert(op.uri);
			processedUris.insert(op.uri);
			uriCh << op.uri;

			if (count > 0)
				--count;
		}
		else if (!op.add) {
			processUris.erase(op.uri);
		}
	}

	uriCh.Close();
	opCh.Close();

	hn_sleep 10000;
}
