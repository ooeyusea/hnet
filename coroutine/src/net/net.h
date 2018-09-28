#ifndef __NET_H__
#define __NET_H__
#include "util.h"
#include "spin_mutex.h"

#define MAX_SOCKET 0xFFFF
#define MAX_ACCEPTOR 0xFFFF

#ifdef WIN32
typedef SOCKET socket_t;
#else
typedef int32_t socket_t;
#endif

namespace hyper_net {
	class Coroutine;

#ifdef WIN32
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
		sockaddr_in remote;
	};

	struct IocpAcceptor {
		IocpEvent accept;
		socket_t sock;
		char buf[128];
	};
#else
	enum {
		EPOLL_OPT_CONNECT = 0,
		EPOLL_OPT_ACCEPT
		EPOLL_OPT_IO
	};

	struct EpollEvent {
		int8_t opt;

		sock_t sock;
		int32_t fd;
	};

	struct EpollConnector {
		EpollEvent connect;

		Coroutine* co;
	};

	struct EpollAcceptor {
		spin_mutex lock;

		int32_t fd;
		socket_t sock;

		bool comming;
		std::list<Coroutine *> waitQueue;
	};
#endif

	class NetEngine {
		struct NetBuffer {
			NetBuffer * next;
			int32_t size;
			char data[1];
		};

		struct Socket {
			spin_mutex lock;
			bool acceptor;

			int32_t fd;
			socket_t sock;

			bool recving;
			char * recvBuf;
			int32_t recvSize;
			int32_t recvMaxSize;
			Coroutine * readingCo;

			bool sending;
			char * sendBuf;
			int32_t sendSize;
			int32_t sendMaxSize;
			NetBuffer * sendChain;
			int32_t sendChainSize;

			bool closing;
			bool closed;
#ifdef WIN32
			IocpEvent evtRecv;
			IocpEvent evtSend;
#endif

			std::list<socket_t> accepts;
			std::list<Coroutine*> waitAcceptCo;
		};

	public:
		static NetEngine& Instance() {
			static NetEngine g_instance;
			return g_instance;
		}

		int32_t Listen(const char * ip, const int32_t port);
		int32_t Connect(const char * ip, const int32_t port);

		int32_t Accept(int32_t fd);

		void Send(int32_t fd, const char * buf, int32_t size);
		int32_t Recv(int32_t fd, char * buf, int32_t size);

		void Shutdown(int32_t fd);
		void Close(int32_t fd);

		void ThreadProc();

#ifdef WIN32
		bool DoSend(Socket & sock);
		bool DoRecv(Socket & sock, int32_t size);
		bool DoAccept(IocpAcceptor * evt);

		IocpEvent * GetQueueState(HANDLE completionPort);
		void DealAccept(IocpAcceptor * evt);
		void DealConnect(IocpConnector * evt);
		void DealSend(IocpEvent * evt);
		void DealRecv(IocpEvent * evt);
#endif

	private:
		NetEngine();
		~NetEngine();

		int32_t Apply(socket_t sock, bool acceptor = false);
		void ShutdownListener(std::unique_lock<spin_mutex>& guard, Socket& sock);

		Socket _sockets[MAX_SOCKET + 1];
		int32_t _nextFd;

		bool _terminate;

#ifdef WIN32
		HANDLE _completionPort;
#else
		EpollAcceptor _acceptor[MAX_ACCEPTOR + 1];
		int32_t _nextAcceptFd;
#endif
	};
}

#endif // !__NET_H__
