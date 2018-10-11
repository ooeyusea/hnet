#ifndef __NET_H__
#define __NET_H__
#include "util.h"
#include "spin_mutex.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>

#define MAX_SOCKET 0xFFFF

typedef int32_t socket_t;
#define INVALID_SOCKET -1

namespace hyper_net {
	class Coroutine;

	enum {
		EPOLL_OPT_CONNECT = 0,
		EPOLL_OPT_ACCEPT,
		EPOLL_OPT_IO,
	};

	struct EpollEvent {
		int8_t opt;

		socket_t sock;
		int32_t fd;
	};

	struct EpollConnector {
		EpollEvent connect;

		Coroutine* co;
	};

	struct EpollWorker {
		int32_t epollFd;
		std::atomic<int32_t> count;
	};

	class NetEngine {
		struct NetBuffer {
			NetBuffer * next;
			int32_t size;
			char data[1];
		};

		struct Socket {
			spin_mutex lock;
			bool acceptor = false;

			int32_t fd = 0;
			socket_t sock = INVALID_SOCKET;
			bool ipv6 = false;

			Coroutine * readingCo = nullptr;

			bool sending = false;
			char * sendBuf = nullptr;
			int32_t sendSize = 0;
			int32_t sendMaxSize = 0;

			bool closing;

			int32_t epollFd;
			EpollEvent evt;

			bool waiting = false;
			std::list<Coroutine*> waitAcceptCo;
		};

	public:
		static NetEngine& Instance() {
			static NetEngine g_instance;
			return g_instance;
		}

		int32_t Listen(const char * ip, const int32_t port, int32_t proto);
		int32_t Connect(const char * ip, const int32_t port, int32_t proto);

		int32_t Accept(int32_t fd, char * remoteIp, int32_t remoteIpSize, int32_t * remotePort);

		void Send(int32_t fd, const char * buf, int32_t size);
		int32_t Recv(int32_t fd, char * buf, int32_t size);

		void Shutdown(int32_t fd);
		void Close(int32_t fd);

		void ThreadProc(EpollWorker& worker);
		void DealConnect(EpollEvent * evt, int32_t flag, int32_t epollFd);
		void DealAccept(EpollEvent * evt, int32_t flag);
		void DealIO(EpollEvent * evt, int32_t flag);

		int32_t AddToWorker(EpollEvent * evt);

	private:
		NetEngine();
		~NetEngine();

		int32_t Apply(socket_t sock, bool acceptor = false, bool ipv6 = false);
		void Close(Socket & sock);
		void ShutdownListener(std::unique_lock<spin_mutex>& guard, Socket& sock);

		Socket _sockets[MAX_SOCKET + 1];
		int32_t _nextFd;

		bool _terminate;
		std::vector<EpollWorker *> _workers;
	};
}

#endif // !__NET_H__
