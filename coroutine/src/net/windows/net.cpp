#include "net.h"
#include "coroutine.h"
#include "hnet.h"
#include <mswsock.h> 
#include "scheduler.h"

#define NET_INIT_FRAME 1024
#define MAX_NET_THREAD 4

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
	NetEngine::NetEngine() {
		_nextFd = 1;

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

		_terminate = false;
	}

	NetEngine::~NetEngine() {
		CloseHandle(_completionPort);
		WSACleanup();
	}

	socket_t NetEngine::Listen(const char * ip, const int32_t port) {
		socket_t sock = INVALID_SOCKET;
		if (INVALID_SOCKET == (sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			return -1;
		}

		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if ((addr.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE) {
			closesocket(sock);
			return -1;
		}

		if (SOCKET_ERROR == bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
			closesocket(sock);
			return -1;
		}

		if (listen(sock, 128) == SOCKET_ERROR) {
			closesocket(sock);
			return -1;
		}

		if (_completionPort != CreateIoCompletionPort((HANDLE)sock, _completionPort, sock, 0)) {
			closesocket(sock);
			return -1;
		}

		return sock;
	}

	int32_t NetEngine::Connect(const char * ip, const int32_t port) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		socket_t sock = INVALID_SOCKET;
		if (INVALID_SOCKET == (sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
			OASSERT(false, "Connect error %d", ::WSAGetLastError());
			return -1;
		}

		DWORD value = 0;
		if (SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &value)) {
			closesocket(sock);
			return -1;
		}

		const int8_t nodelay = 1;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

		if (_completionPort != CreateIoCompletionPort((HANDLE)sock, _completionPort, sock, 0)) {
			closesocket(sock);
			return -1;
		}

		IocpConnector * connector = new IocpConnector;
		connector->co = co;
		SafeMemset(&connector->remote, sizeof(connector->remote), 0, sizeof(connector->remote));

		SafeMemset(&connector->connect, sizeof(connector->connect), 0, sizeof(connector->connect));
		connector->connect.opt = IOCP_OPT_CONNECT;
		connector->connect.sock = sock;
		connector->connect.code = 0;

		connector->remote.sin_family = AF_INET;
		if (SOCKET_ERROR == bind(sock, (sockaddr *)&connector->remote, sizeof(sockaddr_in))) {
			delete connector;
			return -1;
		}

		connector->remote.sin_port = htons(port);
		if ((connector->remote.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE) {
			delete connector;
			return -1;
		}

		BOOL res = g_connect(sock, (sockaddr *)&connector->remote, sizeof(sockaddr_in), nullptr, 0, nullptr, (LPOVERLAPPED)&connector->connect);
		int32_t errCode = WSAGetLastError();
		if (!res && errCode != WSA_IO_PENDING) {
			delete connector;
			return -1;
		}

		co->SetTemp(&sock);
		co->SetStatus(CoroutineState::CS_BLOCK);

		hn_yield;

		delete connector;
		if (sock == INVALID_SOCKET)
			return -1;

		int32_t fd = Apply(sock);
		if (fd < 0) {
			closesocket(sock);
			return -1;
		}

		return fd;
	}

	int32_t NetEngine::Accept(socket_t sock) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		IocpAcceptor * acceptor = new IocpAcceptor;
		acceptor->co = co;

		SafeMemset(&acceptor->accept, sizeof(acceptor->accept), 0, sizeof(acceptor->accept));
		acceptor->accept.opt = IOCP_OPT_ACCEPT;
		acceptor->accept.sock = sock;
		acceptor->accept.code = 0;

		acceptor->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (acceptor->sock == INVALID_SOCKET) {
			OASSERT(false, "do accept failed %d", WSAGetLastError());
			delete acceptor;
			return -1;
		}

		DWORD bytes = 0;
		int32_t res = g_accept(sock,
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
			closesocket(acceptor->sock);
			delete acceptor;
			return -1;
		}

		co->SetTemp(&acceptor->sock);
		co->SetStatus(CoroutineState::CS_BLOCK);

		hn_yield;

		if (acceptor->sock == INVALID_SOCKET) {
			delete acceptor;
			return -1;
		}

		int32_t fd = Apply(acceptor->sock);
		if (fd < 0) {
			closesocket(acceptor->sock);
			delete acceptor;
			return -1;
		}

		delete acceptor;
		return fd;
	}

	void NetEngine::Send(int32_t fd, const char * buf, int32_t size) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		NetBuffer * frame = (NetBuffer *)malloc(sizeof(NetBuffer) + size);
		frame->next = nullptr;
		frame->size = size;
		SafeMemcpy(frame->data, size, buf, size);

		Socket& sock = _sockets[fd & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && !sock.closing && !sock.closed) {
			frame->next = sock.sendChain;
			sock.sendChain = frame;
			sock.sendChainSize += frame->size;

			if (!sock.sending)
				DoSend(sock);
		}
		else {
			guard.unlock();

			free(frame);
		}
	}

	int32_t NetEngine::Recv(int32_t fd, char * buf, int32_t size) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		while (true) {
			Socket& sock = _sockets[fd & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == fd && (sock.readingCo == nullptr || sock.readingCo == co) && !sock.closing && !sock.closed) {
				if (sock.recvSize > 0) {
					int32_t len = size > sock.recvSize ? sock.recvSize : size;
					SafeMemcpy(buf, size, sock.recvBuf, len);
					if (sock.recvSize > len) {
						memmove(sock.recvBuf, sock.recvBuf + len, sock.recvSize - len);
						sock.recvSize -= len;
					}
					sock.readingCo = nullptr;
					return len;
				}
				else {
					co->SetStatus(CoroutineState::CS_BLOCK);

					sock.readingCo = co;
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
		Socket& sock = _sockets[fd & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && !sock.closed) {
			closesocket(sock.sock);
			sock.closed = true;
		}
	}

	void NetEngine::Close(int32_t fd) {
		Socket& sock = _sockets[fd & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && !sock.closing && !sock.closed) {
			sock.closing = true;
			if (!sock.sending) {
				closesocket(sock.sock);
				sock.closed = true;
			}
		}
	}

	void NetEngine::ShutdownListener(socket_t sock) {
		closesocket(sock);
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

	void NetEngine::DealAccept(IocpAcceptor * evt) {
		BOOL res = setsockopt(evt->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char *)&evt->accept.sock, sizeof(evt->accept.sock));

		sockaddr_in remote;
		int32_t size = sizeof(sockaddr_in);
		if (res != 0 || 0 != getpeername(evt->sock, (sockaddr*)&remote, &size)) {
			OASSERT(false, "complete accept error %d", GetLastError());
			closesocket(evt->sock);

			evt->sock = INVALID_SOCKET;
		}
		else {
			DWORD value = 0;
			if (SOCKET_ERROR == ioctlsocket(evt->sock, FIONBIO, &value)) {
				closesocket(evt->sock);

				evt->sock = INVALID_SOCKET;
			}
			else {
				HANDLE ret = CreateIoCompletionPort((HANDLE)evt->sock, _completionPort, evt->sock, 0);
				if (_completionPort != ret) {
					closesocket(evt->sock);

					evt->sock = INVALID_SOCKET;
				}
				else {
					const int8_t nodelay = 1;
					setsockopt(evt->sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
				}
			}
		}

		Scheduler::Instance().AddCoroutine(evt->co);
	}

	void NetEngine::DealConnect(IocpConnector * evt) {
		if (evt->connect.code == ERROR_SUCCESS) {
			const int8_t nodelay = 1;
			setsockopt(evt->connect.sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
		}
		else {
			closesocket(evt->connect.sock);

			socket_t& sock = *(socket_t*)evt->co->GetTemp();
			sock = INVALID_SOCKET;
		}

		Scheduler::Instance().AddCoroutine(evt->co);
	}

	void NetEngine::DealSend(IocpEvent * evt) {
		Socket& sock = _sockets[evt->socket & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == evt->socket) {
			sock.sending = false;

			if (sock.closed) {
				if (!sock.recving) {
					sock.fd = 0;
					sock.sock = INVALID_SOCKET;

					free(sock.sendBuf);
					free(sock.recvBuf);
				}
			}
			else {
				if (evt->code == ERROR_SUCCESS) {
					sock.sendSize -= evt->bytes;
					if (sock.sendSize > 0) {
						memmove(sock.sendBuf, sock.sendBuf + evt->bytes, sock.sendSize);
					}

					if (sock.sendSize > 0 || sock.sendChainSize > 0)
						DoSend(sock);
					else {
						if (sock.closing) {
							closesocket(sock.sock);
							sock.closed = true;
						}
					}
				}
				else {
					if (evt->code != ERROR_IO_PENDING) {
						closesocket(sock.sock);
						sock.closed = true;
					}
					else if (sock.sendSize > 0 || sock.sendChainSize > 0)
						DoSend(sock);
				}
			}
		}
	}

	void NetEngine::DealRecv(IocpEvent * evt) {
		Coroutine * co = nullptr;
		{
			Socket& sock = _sockets[evt->socket & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == evt->socket) {
				sock.recving = false;
				co = sock.readingCo;

				if (evt->code == ERROR_SUCCESS && evt->bytes > 0)
					sock.recvSize += evt->bytes;
				else {
					if (!sock.closed) {
						closesocket(sock.sock);
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
		

		Scheduler::Instance().AddCoroutine(co);
	}

	bool NetEngine::DoSend(Socket & sock) {
		if (sock.sendSize == 0) {
			if (sock.sendChainSize > sock.sendMaxSize) {
				sock.sendMaxSize *= 2;
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

	int32_t NetEngine::Apply(socket_t s) {
		int32_t count = MAX_SOCKET;
		while (count--) {
			int32_t fd = _nextFd;

			_nextFd++;
			if (_nextFd < 0)
				_nextFd = 1;

			Socket& sock = _sockets[fd & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock, std::defer_lock);
			if (guard.try_lock()) {
				if (sock.fd == 0) {
					sock.fd = fd;
					sock.sock = s;

					guard.unlock();

					sock.recving = false;
					sock.recvBuf = (char *)malloc(NET_INIT_FRAME);
					sock.recvSize = 0;
					sock.recvMaxSize = NET_INIT_FRAME;
					sock.readingCo = nullptr;

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
					return fd;
				}
			}
		}
		return -1;
	}
}
