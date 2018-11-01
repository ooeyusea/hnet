#ifndef __HNET_NET_H__
#define __HNET_NET_H__

#define HN_IPV4 0
#define HN_IPV6 1

namespace hyper_net {
	struct NetAdapter {
		int32_t Connect(const char * ip, const int32_t port, int32_t proto = HN_IPV4);
		int32_t Listen(const char * ip, const int32_t port, int32_t proto = HN_IPV4);
		int32_t Accept(int32_t fd, char * remoteIp = nullptr, int32_t remoteIpSize = 0, int32_t * remotePort = nullptr);
		int32_t Recv(int32_t fd, char * buf, int32_t size);
		void Send(int32_t fd, const char * buf, int32_t size);
		void Close(int32_t fd);
		void Shutdown(int32_t fd);
	};
}

#define hn_connect hyper_net::NetAdapter().Connect
#define hn_listen hyper_net::NetAdapter().Listen
#define hn_accept hyper_net::NetAdapter().Accept
#define hn_recv hyper_net::NetAdapter().Recv
#define hn_send hyper_net::NetAdapter().Send
#define hn_close hyper_net::NetAdapter().Close
#define hn_shutdown hyper_net::NetAdapter().Shutdown

#endif // !__HNET_NET_H__
