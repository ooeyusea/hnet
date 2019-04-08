#include "net.h"
#include "coroutine.h"
#include "hnet.h"
#include <mswsock.h> 
#include "scheduler.h"
#include <ws2tcpip.h>
#include "options.h"

#define NET_INIT_FRAME 1024
#define MAX_NET_THREAD 4
#define BACKLOG 128

LPFN_ACCEPTEX GetAcceptExFunc() {
	GUID acceptExGuild = WSAID_ACCEPTEX;
	DWORD bytes = 0;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	LPFN_ACCEPTEX acceptFunc = nullptr;

	WSAIoctl(sock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&acceptExGuild,
		sizeof(acceptExGuild),
		&acceptFunc,
		sizeof(acceptFunc),
		&bytes, nullptr, nullptr);

	if (nullptr == acceptFunc) {
		OASSERT(false, "Get AcceptEx fun error, error code : %d", WSAGetLastError());
	}

	return acceptFunc;
}

LPFN_CONNECTEX GetConnectExFunc() {
	GUID conectExFunc = WSAID_CONNECTEX;
	DWORD bytes = 0;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	LPFN_CONNECTEX connectFunc = nullptr;

	WSAIoctl(sock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&conectExFunc,
		sizeof(conectExFunc),
		&connectFunc,
		sizeof(connectFunc),
		&bytes, nullptr, nullptr);

	if (nullptr == connectFunc) {
		OASSERT(false, "Get ConnectEx fun error, error code : %d", WSAGetLastError());
	}

	return connectFunc;
}

LPFN_ACCEPTEX g_accept = nullptr;
LPFN_CONNECTEX g_connect = nullptr;

namespace hyper_net {
	inline void CloseSocket(SOCKET sock) {
		closesocket(sock);

		//printf("closesocket %lld\n", sock);
	}

	inline int32_t NextP2(int32_t a) {
		int32_t rval = 1;
		while (rval < a)
			rval <<= 1;
		return rval;

	}

	NetEngine::NetEngine() {
		_nextFd = 1;
		_maxSocket = NextP2(Options::Instance().GetMaxSocketCount()) - 1;
		_sockets = new Socket[_maxSocket + 1];

		WSADATA wsaData;
		if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
			throw std::runtime_error("setup net failed");

		if (nullptr == (g_accept = GetAcceptExFunc())) 
			throw std::runtime_error("get accept func failed");

		if (nullptr == (g_connect = GetConnectExFunc()))
			throw std::runtime_error("get connect func failed");

		if (nullptr == (_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)))
			throw std::runtime_error("create completion port failed");

		int32_t cpuCount = (int32_t)std::thread::hardware_concurrency();
		for (int32_t i = 0; i < MAX_NET_THREAD && i < cpuCount * 2; ++i) {
			std::thread([this]() {
				ThreadProc();
			}).detach();
		}

		std::thread([this]() {
			CheckRecvTimeout();
		}).detach();

		_terminate = false;
	}

	NetEngine::~NetEngine() {
		CloseHandle(_completionPort);
		WSACleanup();

		_terminate = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		//delete _sockets;
		//_sockets = nullptr;
	}

	int32_t NetEngine::Listen(const char * ip, const int32_t port, int32_t proto) {
		socket_t sock = INVALID_SOCKET;
		if (proto == HN_IPV4) {
			if (INVALID_SOCKET == (sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
				return -1;
			}

			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			if ((addr.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE) {
				CloseSocket(sock);
				return -1;
			}

			if (SOCKET_ERROR == bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
				CloseSocket(sock);
				return -1;
			}
		}
		else {
			if (INVALID_SOCKET == (sock = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
				return -1;
			}

			sockaddr_in6 addr;
			addr.sin6_family = AF_INET;
			addr.sin6_port = htons(port);
			
			if (inet_pton(AF_INET6, ip, &addr.sin6_addr) != 1) {
				CloseSocket(sock);
				return -1;
			}

			if (SOCKET_ERROR == bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in6))) {
				CloseSocket(sock);
				return -1;
			}
		}

		if (listen(sock, 128) == SOCKET_ERROR) {
			CloseSocket(sock);
			return -1;
		}

		if (_completionPort != CreateIoCompletionPort((HANDLE)sock, _completionPort, sock, 0)) {
			CloseSocket(sock);
			return -1;
		}

		int32_t fd = Apply(sock, true, proto == HN_IPV6);
		if (fd < 0) {
			CloseSocket(sock);
			return -1;
		}

		for (int32_t i = 0; i < BACKLOG; ++i) {
			IocpAcceptor * acceptor = new IocpAcceptor;

			SafeMemset(&acceptor->accept, sizeof(acceptor->accept), 0, sizeof(acceptor->accept));
			acceptor->accept.opt = IOCP_OPT_ACCEPT;
			acceptor->accept.sock = sock;
			acceptor->accept.code = 0;
			acceptor->accept.socket = fd;

			if (!DoAccept(acceptor)) {
				Shutdown(fd);
				return -1;
			}
		}

		return fd;
	}

	int32_t NetEngine::Connect(const char * ip, const int32_t port, int32_t proto) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		IocpConnector connector;
		connector.co = co;

		socket_t sock = INVALID_SOCKET;
		if (proto == HN_IPV4) {
			if (INVALID_SOCKET == (sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
				OASSERT(false, "Connect error %d", ::WSAGetLastError());
				return -1;
			}

			DWORD value = 0;
			if (SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &value)) {
				CloseSocket(sock);
				return -1;
			}

			const int8_t nodelay = 1;
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

			if (_completionPort != CreateIoCompletionPort((HANDLE)sock, _completionPort, sock, 0)) {
				CloseSocket(sock);
				return -1;
			}

			sockaddr_in remote;
			SafeMemset(&remote, sizeof(remote), 0, sizeof(remote));

			SafeMemset(&connector.connect, sizeof(connector.connect), 0, sizeof(connector.connect));
			connector.connect.opt = IOCP_OPT_CONNECT;
			connector.connect.sock = sock;
			connector.connect.code = 0;

			remote.sin_family = AF_INET;
			if (SOCKET_ERROR == bind(sock, (sockaddr *)&remote, sizeof(sockaddr_in)))
				return -1;

			remote.sin_port = htons(port);
			if ((remote.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE)
				return -1;

			BOOL res = g_connect(sock, (sockaddr *)&remote, sizeof(sockaddr_in), nullptr, 0, nullptr, (LPOVERLAPPED)&connector.connect);
			int32_t errCode = WSAGetLastError();
			if (!res && errCode != WSA_IO_PENDING)
				return -1;
		}
		else {
			if (INVALID_SOCKET == (sock = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
				OASSERT(false, "Connect error %d", ::WSAGetLastError());
				return -1;
			}

			DWORD value = 0;
			if (SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &value)) {
				CloseSocket(sock);
				return -1;
			}

			const int8_t nodelay = 1;
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

			if (_completionPort != CreateIoCompletionPort((HANDLE)sock, _completionPort, sock, 0)) {
				CloseSocket(sock);
				return -1;
			}

			sockaddr_in6 remote;
			SafeMemset(&remote, sizeof(remote), 0, sizeof(remote));

			SafeMemset(&connector.connect, sizeof(connector.connect), 0, sizeof(connector.connect));
			connector.connect.opt = IOCP_OPT_CONNECT;
			connector.connect.sock = sock;
			connector.connect.code = 0;

			remote.sin6_family = AF_INET6;
			if (SOCKET_ERROR == bind(sock, (sockaddr *)&remote, sizeof(sockaddr_in6)))
				return -1;

			remote.sin6_port = htons(port);
			if (inet_pton(AF_INET6, ip, &remote.sin6_addr) != 1)
				return -1;

			BOOL res = g_connect(sock, (sockaddr *)&remote, sizeof(sockaddr_in6), nullptr, 0, nullptr, (LPOVERLAPPED)&connector.connect);
			int32_t errCode = WSAGetLastError();
			if (!res && errCode != WSA_IO_PENDING)
				return -1;
		}

		co->SetTemp(&sock);
		co->SetStatus(CoroutineState::CS_BLOCK);

		hn_yield;

		if (sock == INVALID_SOCKET)
			return -1;

		int32_t fd = Apply(sock);
		if (fd < 0) {
			CloseSocket(sock);
			return -1;
		}

		return fd;
	}

	int32_t NetEngine::Accept(int32_t fd, char * remoteIp, int32_t remoteIpSize, int32_t * remotePort) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		while (true) {
			Socket& sock = _sockets[fd & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == fd && sock.acceptor) {
				if (!sock.accepts.empty()) {
					bool ipv6 = sock.ipv6;
					int32_t coming = sock.accepts.front();
					sock.accepts.pop_front();
					guard.unlock();

					if (!ipv6) {
						sockaddr_in remote;
						int32_t len = sizeof(remote);
						getpeername(coming, (sockaddr*)&remote, &len);

						if (remotePort)
							*remotePort = ntohs(remote.sin_port);

						if (remoteIp)
							inet_ntop(AF_INET, &remote.sin_addr, remoteIp, remoteIpSize);
					}
					else {
						sockaddr_in6 remote;
						int32_t len = sizeof(remote);
						getpeername(coming, (sockaddr*)&remote, &len);

						if (remotePort)
							*remotePort = ntohs(remote.sin6_port);

						if (remoteIp)
							inet_ntop(AF_INET6, &remote.sin6_addr, remoteIp, remoteIpSize);
					}

					int32_t comingFd = Apply(coming);
					if (comingFd < 0) {
						CloseSocket(coming);
						return -1;
					}

					return comingFd;
				}
				else {
					sock.waitAcceptCo.push_back(co);
					co->SetStatus(CoroutineState::CS_BLOCK);
					guard.unlock();

					hn_yield;
				}
			}
			else {
				guard.unlock();
				return -1;
			}
		}

		return -1;
	}

	void NetEngine::Send(int32_t fd, const char * buf, int32_t size) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		NetBuffer * frame = (NetBuffer *)malloc(sizeof(NetBuffer) + size);
		frame->next = nullptr;
		frame->size = size;
		SafeMemcpy(frame->data, size, buf, size);

		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && !sock.acceptor && !sock.closing && !sock.closed) {
			frame->next = sock.sendChain;
			sock.sendChain = frame;
			sock.sendChainSize += frame->size;

			if (!sock.sending) {
				if (!DoSend(sock)) {
					CloseSocket(sock.sock);
					sock.closed = true;

					if (!sock.recving) {
						sock.fd = 0;
						sock.sock = INVALID_SOCKET;

						free(sock.sendBuf);
						free(sock.recvBuf);
					}
				}
			}
		}
		else {
			guard.unlock();

			free(frame);
		}
	}

	int32_t NetEngine::Recv(int32_t fd, char * buf, int32_t size, int64_t timeout) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		while (true) {
			Socket& sock = _sockets[fd & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == fd && !sock.acceptor && (sock.readingCo == nullptr || sock.readingCo == co) && !sock.closing && !sock.closed) {
				if (sock.recvSize > 0) {
					int32_t len = size > sock.recvSize ? sock.recvSize : size;
					SafeMemcpy(buf, size, sock.recvBuf, len);
					if (sock.recvSize > len) {
						memmove(sock.recvBuf, sock.recvBuf + len, sock.recvSize - len);
						sock.recvSize -= len;
					}
					else
						sock.recvSize = 0;

					sock.readingCo = nullptr;
					sock.isRecvTimeout = false;
					sock.recvTimeout = 0;
					return len;
				}
				else if (sock.isRecvTimeout) {
					sock.readingCo = nullptr;
					sock.isRecvTimeout = false;
					sock.recvTimeout = 0;
					return -2;
				}
				else {
					co->SetStatus(CoroutineState::CS_BLOCK);

					sock.readingCo = co;
					if (timeout > 0) {
						sock.recvTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + timeout;
					}

					if (!sock.recving && !DoRecv(sock, size)) {
						co->SetStatus(CoroutineState::CS_RUNNABLE);
						return -1;
					}

					guard.unlock();
					
					hn_yield;
				}
			}
			else {
				guard.unlock();
				return -1;
			}
		}
		return -1;
	}

	void NetEngine::Shutdown(int32_t fd) {
		if (fd <= 0)
			return;

		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				if (!sock.closed) {
					CloseSocket(sock.sock);
					sock.closed = true;
				}
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	bool NetEngine::Test(int32_t fd) {
		Socket& sock = _sockets[fd & _maxSocket];
		if (sock.fd == fd) {
			if (!sock.closed && !sock.closing)
				return true;
		}
		return false;
	}

	void NetEngine::Close(int32_t fd) {
		if (fd <= 0)
			return;

		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				if (!sock.closing && !sock.closed) {
					sock.closing = true;
					if (!sock.sending) {
						CloseSocket(sock.sock);
						sock.closed = true;
					}
				}
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	void NetEngine::ThreadProc() {
		while (!_terminate) {
			IocpEvent * evt = GetQueueState(_completionPort);
			if (evt) {
				switch (evt->opt) {
				case IOCP_OPT_ACCEPT: DealAccept((IocpAcceptor *)evt); break;
				case IOCP_OPT_CONNECT: DealConnect((IocpConnector *)evt); break;
				case IOCP_OPT_RECV: DealRecv(evt); break;
				case IOCP_OPT_SEND: DealSend(evt); break;
				}
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	void NetEngine::CheckRecvTimeout() {
		while (!_terminate) {
			int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			for (int32_t i = 0; i < _maxSocket; ++i) {
				Socket& sock = _sockets[i];
				if (sock.fd != 0 && !sock.acceptor && sock.readingCo && !sock.closing && !sock.closed && sock.recvTimeout > 0 && now > sock.recvTimeout) {
					Coroutine * co = nullptr;
					{
						std::unique_lock<spin_mutex> guard(sock.lock);
						if (sock.fd != 0 && !sock.acceptor && sock.readingCo && !sock.closing && !sock.closed && sock.recvTimeout > 0 && now > sock.recvTimeout) {
							co = sock.readingCo;

							sock.recvTimeout = 0;
							sock.isRecvTimeout = true;
						}
					}

					if (co)
						Scheduler::Instance().AddCoroutine(co);
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void NetEngine::DealAccept(IocpAcceptor * evt) {
		if (evt->accept.code == ERROR_SUCCESS) {
			BOOL res = setsockopt(evt->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char *)&evt->accept.sock, sizeof(evt->accept.sock));

			sockaddr_in remote;
			int32_t size = sizeof(sockaddr_in);
			if (res != 0 || 0 != getpeername(evt->sock, (sockaddr*)&remote, &size)) {
				//OASSERT(false, "complete accept error %d", GetLastError());
				CloseSocket(evt->sock);

				evt->sock = INVALID_SOCKET;
			}
			else {
				DWORD value = 0;
				if (SOCKET_ERROR == ioctlsocket(evt->sock, FIONBIO, &value)) {
					CloseSocket(evt->sock);

					evt->sock = INVALID_SOCKET;
				}
				else {
					HANDLE ret = CreateIoCompletionPort((HANDLE)evt->sock, _completionPort, evt->sock, 0);
					if (_completionPort != ret) {
						CloseSocket(evt->sock);

						evt->sock = INVALID_SOCKET;
					}
					else {
						const int8_t nodelay = 1;
						setsockopt(evt->sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
					}
				}
			}

			int32_t fd = evt->accept.socket;
			socket_t sock = evt->sock;
			if (sock != INVALID_SOCKET) {
				Socket& acceptSock = _sockets[fd & _maxSocket];
				std::unique_lock<spin_mutex> guard(acceptSock.lock);
				if (acceptSock.fd == fd) {
					acceptSock.accepts.push_back(sock);

					if (!acceptSock.waitAcceptCo.empty()) {
						Coroutine * co = acceptSock.waitAcceptCo.front();
						acceptSock.waitAcceptCo.pop_front();
						guard.unlock();

						Scheduler::Instance().AddCoroutine(co);
					}
				}
				else {
					guard.unlock();

					delete evt;
					return;
				}
			}

			if (!DoAccept(evt))
				Shutdown(fd);
		}
		else {
			//printf("shutdown listener\n");
			CloseSocket(evt->sock);
			evt->sock = INVALID_SOCKET;

			Shutdown(evt->accept.socket);
			delete evt;
		}
	}

	void NetEngine::DealConnect(IocpConnector * evt) {
		if (evt->connect.code == ERROR_SUCCESS) {
			const int8_t nodelay = 1;
			setsockopt(evt->connect.sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
		}
		else {
			CloseSocket(evt->connect.sock);

			socket_t& sock = *(socket_t*)evt->co->GetTemp();
			sock = INVALID_SOCKET;
		}

		Scheduler::Instance().AddCoroutine(evt->co);
	}

	void NetEngine::DealSend(IocpEvent * evt) {
		//hn_info("send {}:{}", evt->socket, evt->code);
		Socket& sock = _sockets[evt->socket & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == evt->socket) {
			sock.sending = false;

			if (!sock.closed) {
				if (evt->code == ERROR_SUCCESS) {
					sock.sendSize -= evt->bytes;
					if (sock.sendSize > 0) {
						memmove(sock.sendBuf, sock.sendBuf + evt->bytes, sock.sendSize);
					}

					//printf("send %lld[%d]:%d->%d %d\n", evt->sock, sock.closing, sock.sendSize, sock.sendChainSize, sock.sendSize > 0 || sock.sendChainSize > 0);
					if (sock.sendSize > 0 || sock.sendChainSize > 0) {
						//hn_info("send1 {}[{}]:{}->{} {}", evt->socket, sock.closing, sock.sendSize, sock.sendChainSize, sock.sendSize > 0 || sock.sendChainSize > 0);
						if (!DoSend(sock)) {
							CloseSocket(sock.sock);
							sock.closed = true;
						}
					}
					else {
						//hn_info("send2 {}[{}]:{}->{}", evt->socket, sock.closing, sock.sendSize, sock.sendChainSize);
						if (sock.closing) {
							CloseSocket(sock.sock);
							sock.closed = true;
						}
					}
					//printf("sended\n");
				}
				else {
					if (evt->code != ERROR_IO_PENDING) {
						CloseSocket(sock.sock);
						sock.closed = true;
					}
					else if (sock.sendSize > 0 || sock.sendChainSize > 0) {
						if (!DoSend(sock)) {
							CloseSocket(sock.sock);
							sock.closed = true;
						}
					}
				}
			}

			if (sock.closed) {
				if (!sock.recving) {
					sock.fd = 0;
					sock.sock = INVALID_SOCKET;

					free(sock.sendBuf);
					free(sock.recvBuf);
				}
			}
		}
	}

	void NetEngine::DealRecv(IocpEvent * evt) {
		//hn_info("recv {}:{}", evt->socket, evt->code);
		Coroutine * co = nullptr;
		{
			Socket& sock = _sockets[evt->socket & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == evt->socket) {
				sock.recving = false;
				if (!sock.isRecvTimeout)
					co = sock.readingCo;

				if (evt->code == ERROR_SUCCESS && evt->bytes > 0)
					sock.recvSize += evt->bytes;
				else {
					if (!sock.closed) {
						CloseSocket(sock.sock);
						sock.closed = true;
					}

					if (sock.closed) {
						if (sock.sending == false) {
							sock.fd = 0;
							sock.sock = INVALID_SOCKET;

							free(sock.sendBuf);
							free(sock.recvBuf);
						}
					}
				}
			}
		}
		
		if (co)
			Scheduler::Instance().AddCoroutine(co);
	}

	bool NetEngine::DoSend(Socket & sock) {
		if (sock.sendSize == 0) {
			if (sock.sendChainSize > sock.sendMaxSize) {
				sock.sendMaxSize = sock.sendChainSize * 2;
				sock.sendBuf = (char*)realloc(sock.sendBuf, sock.sendMaxSize);
			}

			NetBuffer * p = sock.sendChain;
			while (p && sock.sendMaxSize - sock.sendSize >= p->size) {
				SafeMemcpy(sock.sendBuf + sock.sendSize, sock.sendMaxSize - sock.sendSize, p->data, p->size);

				NetBuffer * frame = p;
				sock.sendChainSize -= p->size;
				sock.sendSize += p->size;
				p = p->next;

				free(frame);
			}
			sock.sendChain = p;
		}

		sock.evtSend.buf.buf = sock.sendBuf;
		sock.evtSend.buf.len = sock.sendSize;
		sock.evtSend.code = 0;
		sock.evtSend.bytes = 0;
		if (SOCKET_ERROR == WSASend(sock.sock, &sock.evtSend.buf, 1, nullptr, 0, (LPWSAOVERLAPPED)&sock.evtSend, nullptr)) {
			sock.evtSend.code = WSAGetLastError();
			if (WSA_IO_PENDING != sock.evtSend.code)
				return false;
		}
		sock.sending = true;
		return true;
	}


	bool NetEngine::DoRecv(Socket & sock, int32_t size) {
		if (size > sock.recvMaxSize) {
			sock.recvMaxSize *= 2;
			sock.recvBuf = (char*)realloc(sock.recvBuf, sock.recvMaxSize);
		}

		DWORD flags = 0;
		sock.evtRecv.buf.buf = sock.recvBuf + sock.recvSize;
		sock.evtRecv.buf.len = sock.recvMaxSize;
		sock.evtRecv.code = 0;
		sock.evtRecv.bytes = 0;
		if (SOCKET_ERROR == WSARecv(sock.sock, &sock.evtRecv.buf, 1, nullptr, &flags, (LPWSAOVERLAPPED)&sock.evtRecv, nullptr)) {
			sock.evtRecv.code = WSAGetLastError();
			if (WSA_IO_PENDING != sock.evtRecv.code)
				return false;
		}

		sock.recving = true;
		return true;
	}

	bool NetEngine::DoAccept(IocpAcceptor * acceptor) {
		acceptor->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (acceptor->sock == INVALID_SOCKET) {
			OASSERT(false, "do accept failed %d", WSAGetLastError());
			delete acceptor;
			return false;
		}

		//hn_info("DoAccept {} {}", acceptor->accept.sock, acceptor->sock);

		DWORD bytes = 0;
		int32_t res = g_accept(acceptor->accept.sock,
			acceptor->sock,
			acceptor->buf,
			0,
			sizeof(sockaddr_in) + 16,
			sizeof(sockaddr_in) + 16,
			&bytes,
			(LPOVERLAPPED)&acceptor->accept
		);

		int32_t errCode = WSAGetLastError();
		if (!res && errCode != WSA_IO_PENDING) {
			OASSERT(false, "do accept failed %d", errCode);
			CloseSocket(acceptor->sock);
			delete acceptor;
			return false;
		}

		return true;
	}

	IocpEvent * NetEngine::GetQueueState(HANDLE completionPort) {
		DWORD bytes = 0;
		socket_t socket = INVALID_SOCKET;
		IocpEvent * evt = nullptr;

		SetLastError(0);
		BOOL ret = GetQueuedCompletionStatus(completionPort, &bytes, (PULONG_PTR)&socket, (LPOVERLAPPED *)&evt, 10);

		if (nullptr == evt)
			return nullptr;

		evt->code = WSAGetLastError();
		evt->bytes = bytes;
		if (!ret) {
			if (WAIT_TIMEOUT == evt->code)
				return nullptr;
		}
		return evt;
	}

	int32_t NetEngine::Apply(socket_t s, bool acceptor, bool ipv6) {
		int32_t count = _maxSocket;
		while (count--) {
			int32_t fd = _nextFd;

			_nextFd++;
			if (_nextFd <= 0)
				_nextFd = 1;

			Socket& sock = _sockets[fd & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock, std::defer_lock);
			if (guard.try_lock()) {
				if (sock.fd == 0) {
					sock.fd = fd;
					sock.sock = s;
					sock.acceptor = acceptor;
					sock.ipv6 = ipv6;

					sock.recving = false;
					sock.recvBuf = (char *)malloc(NET_INIT_FRAME);
					sock.recvSize = 0;
					sock.recvMaxSize = NET_INIT_FRAME;
					sock.readingCo = nullptr;
					sock.recvTimeout = 0;
					sock.isRecvTimeout = false;

					sock.sending = false;
					sock.sendBuf = (char *)malloc(NET_INIT_FRAME);
					sock.sendSize = 0;
					sock.sendMaxSize = NET_INIT_FRAME;
					sock.sendChain = nullptr;
					sock.sendChainSize = 0;

					sock.closing = false;
					sock.closed = false;

					SafeMemset(&sock.evtRecv, sizeof(sock.evtRecv), 0, sizeof(sock.evtRecv));
					sock.evtRecv.opt = IOCP_OPT_RECV;
					sock.evtRecv.socket = fd;
					sock.evtRecv.sock = s;
					sock.evtRecv.bytes = 0;

					SafeMemset(&sock.evtSend, sizeof(sock.evtSend), 0, sizeof(sock.evtSend));
					sock.evtSend.opt = IOCP_OPT_SEND;
					sock.evtSend.socket = fd;
					sock.evtSend.sock = s;
					sock.evtSend.bytes = 0;

					//hn_info("apply fd {} {}", fd, sock.sock);
					return fd;
				}
			}
		}
		return -1;
	}

	void NetEngine::ShutdownListener(std::unique_lock<spin_mutex>& guard, Socket& sock) {
		CloseSocket(sock.sock);
		sock.fd = 0;
		sock.sock = INVALID_SOCKET;


		auto tmp = std::move(sock.waitAcceptCo);
		auto accepts = std::move(sock.accepts);
		guard.unlock();

		for (auto ac : accepts)
			CloseSocket(ac);

		for (auto * co : tmp)
			Scheduler::Instance().AddCoroutine(co);
	}

}
