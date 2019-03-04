#ifndef __NET_H__
#define __NET_H__
#include "util.h"
#include "spin_mutex.h"

typedef SOCKET socket_t;

namespace hyper_net {
	class Coroutine;

	enum {
		IOCP_OPT_CONNECT = 0,
		IOCP_OPT_ACCEPT,
		IOCP_OPT_RECV,
		IOCP_OPT_SEND,
	};

	struct IocpEvent {
		OVERLAPPED ol;
		int8_t opt;
		int32_t code;
		WSABUF buf;
		int32_t socket;
		socket_t sock;
		DWORD bytes;
	};

	struct IocpConnector {
		IocpEvent connect;
		Coroutine * co;
	};

	struct IocpAcceptor {
		IocpEvent accept;
		socket_t sock;
		char buf[128];
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
			bool ipv6 = false;

			int32_t fd = 0;
			socket_t sock = INVALID_SOCKET;

			bool recving;
			char * recvBuf = nullptr;
			int32_t recvSize;
			int32_t recvMaxSize;
			Coroutine * readingCo = nullptr;
			int64_t recvTimeout = 0;
			bool isRecvTimeout = false;

			bool sending;
			char * sendBuf = nullptr;
			int32_t sendSize;
			int32_t sendMaxSize;
			NetBuffer * sendChain = nullptr;
			int32_t sendChainSize;

			bool closing;
			bool closed;

			IocpEvent evtRecv;
			IocpEvent evtSend;

			std::list<socket_t> accepts;
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
		int32_t Recv(int32_t fd, char * buf, int32_t size, int64_t timeout);

		void Shutdown(int32_t fd);
		void Close(int32_t fd);
		bool Test(int32_t fd);

		void ThreadProc();
		void CheckRecvTimeout();

		bool DoSend(Socket & sock);
		bool DoRecv(Socket & sock, int32_t size);
		bool DoAccept(IocpAcceptor * evt);

		IocpEvent * GetQueueState(HANDLE completionPort);
		void DealAccept(IocpAcceptor * evt);
		void DealConnect(IocpConnector * evt);
		void DealSend(IocpEvent * evt);
		void DealRecv(IocpEvent * evt);

	private:
		NetEngine();
		~NetEngine();

		int32_t Apply(socket_t sock, bool acceptor = false, bool ipv6 = false);
		void ShutdownListener(std::unique_lock<spin_mutex>& guard, Socket& sock);

		Socket * _sockets;
		int32_t _nextFd;
		int32_t _maxSocket;

		bool _terminate;

		HANDLE _completionPort;
	};
}

#endif // !__NET_H__
