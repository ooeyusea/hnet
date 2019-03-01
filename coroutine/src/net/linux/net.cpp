#include "net.h"
#include "coroutine.h"
#include "hnet.h"
#include "scheduler.h"
#include "options.h"

#define NET_INIT_FRAME 1024
#define MAX_NET_THREAD 4
#define TIMEOUT_BATCH 1024

namespace hyper_net {
	inline int32_t NextP2 (int32_t a) {
		int32_t rval = 1;
		while(rval < a) 
			rval <<= 1;
		return rval;

	}
	
	NetEngine::NetEngine() {
		_nextFd = 1;
		_maxSocket = NextP2(Options::Instance().GetMaxSocketCount()) - 1;
		_sockets = new Socket[_maxSocket + 1];

		int32_t cpuCount = (int32_t)std::thread::hardware_concurrency();
		int32_t threadCount = (cpuCount > MAX_NET_THREAD) ? MAX_NET_THREAD : cpuCount;
		for (int32_t i = 0; i < threadCount; ++i) {
			EpollWorker * worker = new EpollWorker;
			_workers.push_back(worker);

			worker->epollFd = epoll_create(1024);
			OASSERT(worker->epollFd > 0, "create epoll failed");

			std::thread([this, worker]() {
				ThreadProc(*worker);
			}).detach();
		}
		
		std::thread([this]() {
			CheckRecvTimeout();
		}).detach();

		_terminate = false;
	}

	NetEngine::~NetEngine() {
		_terminate = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		delete [] _sockets;
		_sockets = nullptr;
	}

	int32_t NetEngine::Listen(const char * ip, const int32_t port, int32_t proto) {
		if (Options::Instance().IsDebug()) {
			printf("listen %s:%d %s\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
		}
		
		socket_t sock = INVALID_SOCKET;
		if (proto == HN_IPV4) {
			if (INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s create socket failed %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s set nonblock failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			int32_t reuse = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s set reuse failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s address parse error %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s bind socket error %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}
		}
		else {
			if (INVALID_SOCKET == (sock = socket(AF_INET6, SOCK_STREAM, 0))) {
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s create socket failed %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s set nonblock failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			int32_t reuse = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == -1) {
				close(sock);

				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s set reuse failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			sockaddr_in6 addr;
			addr.sin6_family = AF_INET6;
			addr.sin6_port = htons(port);
			if (inet_pton(AF_INET6, ip, &addr.sin6_addr) != 1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s address parse error %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("listen %s:%d %s bind socket error %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}
		}

		if (listen(sock, 128) < 0) {
			close(sock);
			
			if (Options::Instance().IsDebug()) {
				printf("listen %s:%d %s listen socket error %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
			}
			return -1;
		}

		int32_t fd = Apply(sock, true, proto == HN_IPV6);
		if (fd < 0) {
			close(sock);
			
			if (Options::Instance().IsDebug()) {
				printf("listen %s:%d %s listen apply sock error\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
			}
			return -1;
		}

		if (Options::Instance().IsDebug()) {
			printf("listen %s:%d %s listen apply sock success[%d]\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", fd);
		}
		return fd;
	}

	int32_t NetEngine::Connect(const char * ip, const int32_t port, int32_t proto) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");
		
		if (Options::Instance().IsDebug()) {
			printf("connect %s:%d %s\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
		}

		socket_t sock = INVALID_SOCKET;
		if (proto == HN_IPV4) {
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s parse ip failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			if (INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s create socket failed %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s set nonblock failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			long nonNegal = 1l;
			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s set nodelay failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			int32_t res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
			if (res < 0 && errno != EINPROGRESS) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s set connect failed %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}
		}
		else {
			sockaddr_in6 addr;
			addr.sin6_family = AF_INET6;
			addr.sin6_port = htons(port);
			if (inet_pton(AF_INET6, ip, &addr.sin6_addr) != 1) {
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s parse ip failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			if (INVALID_SOCKET == (sock = socket(AF_INET6, SOCK_STREAM, 0))) {
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s create socket failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}

			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s set nonblock failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			long nonNegal = 1l;
			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s set nodelay failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			int32_t res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
			if (res < 0 && errno != EINPROGRESS) {
				close(sock);
				
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s connect failed %d\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", errno);
				}
				return -1;
			}
		}


		EpollConnector connector;
		connector.connect.opt = EPOLL_OPT_CONNECT;
		connector.connect.sock = sock;
		connector.co = co;

		int32_t error = 0;
		co->SetStatus(CoroutineState::CS_BLOCK);
		co->SetTemp(&error);

		int32_t epollFd = AddToWorker((EpollEvent*)&connector);

		hn_yield;
		
		epoll_ctl(epollFd, EPOLL_CTL_DEL, sock, nullptr);

		if (error == 0) {
			int32_t fd = Apply(sock, false, proto == HN_IPV6);
			if (fd > 0) {
				if (Options::Instance().IsDebug()) {
					printf("connect %s:%d %s success[%d]\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6", fd);
				}
				return fd;
			}
		}

		if (Options::Instance().IsDebug()) {
			printf("connect %s:%d %s finally failed\n", ip, port, proto == HN_IPV4 ? "ipv4" : "ipv6");
		}
		close(sock);
		return -1;
	}

	int32_t NetEngine::Accept(int32_t fd, char * remoteIp, int32_t remoteIpSize, int32_t * remotePort) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && sock.acceptor) {
			bool ipv6 = sock.ipv6;
			socket_t comingSock = INVALID_SOCKET;
			
			if (!sock.waiting) {
				if (sock.ipv6) {
					struct sockaddr_in6 addr;
					socklen_t len = sizeof(addr);

					comingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
				}
				else {
					struct sockaddr_in addr;
					socklen_t len = sizeof(addr);

					comingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
				}
				
				if (comingSock < 0) {
					if (errno == EAGAIN) {
						sock.waiting = true;
					}
					else {
						if (Options::Instance().IsDebug()) {
							printf("accept[%d:%s] failed %d\n", fd, !ipv6 ? "ipv4" : "ipv6", errno);
						}
						return -1;
					}
				}
			}
			
			if (sock.waiting) {
				sock.waitAcceptCo.push_back(co);
				co->SetStatus(CoroutineState::CS_BLOCK);
				co->SetTemp(&comingSock);
				guard.unlock();

				hn_yield;
				
				if (comingSock == INVALID_SOCKET) {
					if (Options::Instance().IsDebug()) {
						printf("accept[%d:%s] failed[background]\n", fd, !ipv6 ? "ipv4" : "ipv6");
					}
					
					return -1;
				}
			}
			else{
				guard.unlock();
			}

			if (fcntl(comingSock, F_SETFL, fcntl(comingSock, F_GETFD, 0) | O_NONBLOCK) == -1) {
				close(comingSock);
				
				if (Options::Instance().IsDebug()) {
					printf("accept[%d:%s] set accepted socket noblock failed\n", fd, !ipv6 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			long nonNegal = 1l;
			if (setsockopt(comingSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
				close(comingSock);

				if (Options::Instance().IsDebug()) {
					printf("accept[%d:%s] set accepted socket nodelay failed\n", fd, !ipv6 ? "ipv4" : "ipv6");
				}
				return -1;
			}

			if (!ipv6) {
				sockaddr_in remote;
				socklen_t len = sizeof(remote);
				getpeername(comingSock, (sockaddr*)&remote, &len);

				if (remotePort)
					*remotePort = ntohs(remote.sin_port);

				if (remoteIp)
					inet_ntop(AF_INET, &remote.sin_addr, remoteIp, remoteIpSize);
			}
			else {
				sockaddr_in6 remote;
				socklen_t len = sizeof(remote);
				getpeername(comingSock, (sockaddr*)&remote, &len);

				if (remotePort)
					*remotePort = ntohs(remote.sin6_port);

				if (remoteIp)
					inet_ntop(AF_INET6, &remote.sin6_addr, remoteIp, remoteIpSize);
			}
			
			int32_t remoteFd = Apply(comingSock);
			if (remoteFd < 0) {
				close(comingSock);

				if (Options::Instance().IsDebug()) {
					printf("accept[%d:%s] apply accepted socket failed\n", fd, !ipv6 ? "ipv4" : "ipv6");
				}
				return -1;
			}
			
			if (Options::Instance().IsDebug()) {
				printf("accept[%d:%s] apply accepted socket[%d] sucess\n", fd, !ipv6 ? "ipv4" : "ipv6", remoteFd);
			}
			
			return remoteFd;
		}
		
		return -1;
	}

	void NetEngine::Send(int32_t fd, const char * buf, int32_t size) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		auto resizeSendBuff = [](Socket & sock, int32_t size) {
			if (sock.sendBuf == nullptr) {
				sock.sendBuf = (char*)malloc(size * 2);
				sock.sendSize = 0;
				sock.sendMaxSize = size * 2;
			}
			else if (sock.sendSize + size > sock.sendMaxSize) {
				sock.sendBuf = (char*)realloc(sock.sendBuf, (sock.sendSize + size) * 2);
				sock.sendMaxSize = (sock.sendSize + size) * 2;
			}
			
			if (Options::Instance().IsDebug()) {
				printf("send[%d] resize send buff %d\n", sock.fd, sock.sendMaxSize);
			}
		};

		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && !sock.acceptor && !sock.closing) {
			if (sock.sending) {
				resizeSendBuff(sock, size);

				SafeMemcpy(sock.sendBuf + sock.sendSize, sock.sendMaxSize - sock.sendSize, buf, size);
				sock.sendSize += size;
			}
			else {
				int32_t offset = 0;
				while (offset < size) {
					int32_t len = send(sock.sock, buf + offset, size - offset, 0);
					if (len <= 0) {
						if (errno == EAGAIN) {
							sock.sending = true;

							resizeSendBuff(sock, size - offset);

							SafeMemcpy(sock.sendBuf + sock.sendSize, sock.sendMaxSize - sock.sendSize, buf + offset, size - offset);
							sock.sendSize += (size - offset);
						}
						else {
							if (Options::Instance().IsDebug()) {
								printf("send [%d] failed %d\n", sock.fd, errno);
							}
							
							auto * readingCo = sock.readingCo;
							bool recving = sock.recving;

							//printf("close2\n");
							Close(sock);
							guard.unlock();

							if (!recving && readingCo)
								Scheduler::Instance().AddCoroutine(readingCo);
						}
						break;
					}
					else
						offset += len;
				}
				
				if (Options::Instance().IsDebug()) {
					printf("send [%d] data size %d rest %d\n", sock.fd, offset, sock.sendSize);
				}
			}
		}
	}

	int32_t NetEngine::Recv(int32_t fd, char * buf, int32_t size, int64_t timeout) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		bool first = true;
		while (true) {
			Socket& sock = _sockets[fd & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == fd && !sock.acceptor && (sock.readingCo == nullptr || sock.readingCo == co) && !sock.closing) {
				if (sock.isRecvTimeout) {
					sock.readingCo = nullptr;
					sock.isRecvTimeout = false;
					sock.recvTimeout = 0;
					
					if (Options::Instance().IsDebug()) {
						printf("recv [%d] timeout %d\n", sock.fd, timeout);
					}
					return -2;
				}

				if (first)
					sock.recvTimeout = 0;
				sock.readingCo = co;
				sock.recving = true;
				sock.wakeupRecv = false;

				socket_t s = sock.sock;
				guard.unlock();
				
				int32_t offset = 0;
				while (true) {
					if (offset >= size) {
						guard.lock();

						if (sock.fd == fd && !sock.acceptor && sock.readingCo == co) {
							sock.readingCo = nullptr;
							sock.recving = false;
						}
						return offset;
					}

					int32_t len = recv(s, buf + offset, size - offset, 0);
					if (len <= 0) {
						guard.lock();

						if (len < 0 && errno == EAGAIN) {
							if (sock.fd == fd && !sock.acceptor && sock.readingCo == co) {
								if (offset == 0) {
									if (!sock.wakeupRecv) {
										sock.recving = false;
										co->SetStatus(CoroutineState::CS_BLOCK);
										if (first && timeout > 0) {
											sock.recvTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + timeout;
											first = false;
										}

										if (Options::Instance().IsDebug()) {
											printf("recv [%d] pending\n", sock.fd);
										}

										guard.unlock();
										hn_yield;
									}
									break;
								}
								else {
									sock.readingCo = nullptr;
									sock.recving = false;
									return offset;
								}
							}
						}

						if (Options::Instance().IsDebug()) {
							printf("recv [%d] error %d\n", fd, errno);
						}
						
						if (sock.fd == fd && !sock.acceptor && sock.readingCo == co)
							Close(sock);
						
						return offset > 0 ? offset : len;
					}
					else
						offset += len;
				}
			}
			else {
				guard.unlock();
				
				if (Options::Instance().IsDebug()) {
					printf("recv [%d] finally failed\n", fd);
				}
				return -1;
			}
		}
		return -1;
	}

	void NetEngine::Shutdown(int32_t fd) {
		if (fd <= 0)
			return;
		
		if (Options::Instance().IsDebug()) {
			printf("shutdown [%d]\n", fd);
		}
		
		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				auto * readingCo = sock.readingCo;
				bool recving = sock.recving;

				Close(sock);
				guard.unlock();

				if (!recving && readingCo)
					Scheduler::Instance().AddCoroutine(readingCo);
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	void NetEngine::Close(int32_t fd) {
		if (fd <= 0)
			return;
		
		if (Options::Instance().IsDebug()) {
			printf("close [%d]\n", fd);
		}
		
		Socket& sock = _sockets[fd & _maxSocket];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				if (!sock.closing) {
					sock.closing = true;
					if (!sock.sending) {
						auto * readingCo = sock.readingCo;
						bool recving = sock.recving;

						//printf("close4\n");
						Close(sock);
						guard.unlock();

						if (!recving && readingCo)
							Scheduler::Instance().AddCoroutine(readingCo);
					}
				}
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	bool NetEngine::Test(int32_t fd) {
		Socket& sock = _sockets[fd & _maxSocket];
		if (sock.fd == fd) {
			if (!sock.closing)
				return true;
		}
		return false;
	}

	void NetEngine::ThreadProc(EpollWorker& worker) {
		epoll_event evs[Options::Instance().GetEpollEventSize()];
		while (!_terminate) {
			int32_t count = epoll_wait(worker.epollFd, evs, Options::Instance().GetEpollEventSize(), 1);
			if (count > 0) {
				for (int32_t i = 0; i < count; ++i) {
					EpollEvent * evt = (EpollEvent *) evs[i].data.ptr;
					switch (evt->opt) {
					case EPOLL_OPT_CONNECT: DealConnect(evt, evs[i].events, worker.epollFd); break;
					case EPOLL_OPT_ACCEPT: DealAccept(evt, evs[i].events); break;
					case EPOLL_OPT_IO: DealIO(evt, evs[i].events); break;
					}
				}
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	void NetEngine::DealConnect(EpollEvent * evt, int32_t flag, int32_t epollFd) {
		EpollConnector * connector = (EpollConnector*)evt;
		if (flag & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			epoll_ctl(epollFd, EPOLL_CTL_DEL, connector->connect.sock, nullptr);

			int32_t& error = *(int32_t*)connector->co->GetTemp();
			error = -1;
			Scheduler::Instance().AddCoroutine(connector->co);
		}
		else if (flag & EPOLLOUT) {
			epoll_ctl(epollFd, EPOLL_CTL_DEL, connector->connect.sock, nullptr);

			Scheduler::Instance().AddCoroutine(connector->co);
		}
	}

	void NetEngine::DealAccept(EpollEvent * evt, int32_t flag) {
		if (flag & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			if (Options::Instance().IsDebug()) {
				int32_t error = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(evt->sock, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
					printf("accept[%d] [background] error %d\n", evt->fd, error);
				}
			}
			
			Shutdown(evt->fd);
		}
		else if (flag & EPOLLIN) {
			Socket& sock = _sockets[evt->fd & _maxSocket];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == evt->fd && sock.acceptor) {
				sock.waiting = false;

				while (!sock.waitAcceptCo.empty()) {
					Coroutine * co = sock.waitAcceptCo.front();
					socket_t& commingSock = *(socket_t*)co->GetTemp();

					if (sock.ipv6) {
						struct sockaddr_in6 addr;
						socklen_t len = sizeof(addr);

						commingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
					}
					else {
						struct sockaddr_in addr;
						socklen_t len = sizeof(addr);

						commingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
					}

					if (commingSock < 0) {
						if (errno == EAGAIN) {
							sock.waiting = true;
						}
						else
							ShutdownListener(guard, sock);
						break;
					}

					sock.waitAcceptCo.pop_front();
					Scheduler::Instance().AddCoroutine(co);
				}
			}
		}
	}

	void NetEngine::DealIO(EpollEvent * evt, int32_t flag) {
		if (flag & (EPOLLERR | EPOLLHUP)) {
			if (Options::Instance().IsDebug()) {
				int32_t error = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(evt->sock, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
					printf("io[%d] [background] error %d\n", evt->fd, error);
				}
			}
			
			Shutdown(evt->fd);
		}
		else {
			bool valid = true;
			if (flag & EPOLLOUT) {
				Socket& sock = _sockets[evt->fd & _maxSocket];
				std::unique_lock<spin_mutex> guard(sock.lock);
				if (sock.fd == evt->fd && !sock.acceptor) {
					sock.sending = false;
					
					int32_t offset = 0;
					while (true) {
						if (offset >= sock.sendSize) {
							sock.sendSize = 0;
							
							if (Options::Instance().IsDebug()) {
								printf("send [%d] data size [backgournd] %d %d\n", sock.fd, offset, sock.sendSize);
							}		
							
							if (sock.closing) {
								auto * readingCo = sock.readingCo;
								bool recving = sock.recving;

								Close(sock);
								guard.unlock();

								if (!recving && readingCo)
									Scheduler::Instance().AddCoroutine(readingCo);
								
								valid = false;
							}
							break;
						}
						
						int32_t len = send(sock.sock, sock.sendBuf + offset, sock.sendSize - offset, 0);
						if (len <= 0) {
							if (errno == EAGAIN) {
								sock.sending = true;
								
								if (offset > 0) {
									memmove(sock.sendBuf, sock.sendBuf + offset, sock.sendSize - offset);
									sock.sendSize -= offset;
								}
								
								if (Options::Instance().IsDebug()) {
									printf("send [%d] data size [backgournd] %d %d\n", sock.fd, offset, sock.sendSize);
								}
							}
							else {
								if (Options::Instance().IsDebug()) {
									printf("send [%d] failed %d\n", sock.fd, errno);
								}
								
								auto * readingCo = sock.readingCo;
								bool recving = sock.recving;

								//printf("close5\n");
								Close(sock);
								guard.unlock();

								if (!recving && readingCo)
									Scheduler::Instance().AddCoroutine(readingCo);
								valid = false;
							}
							break;
						}
						else
							offset += len;
					}
				}
			}

			if (valid && (flag & EPOLLIN)) {
				Socket& sock = _sockets[evt->fd & _maxSocket];
				std::unique_lock<spin_mutex> guard(sock.lock);
				if (sock.fd == evt->fd && !sock.acceptor && !sock.closing) {
					if (!sock.isRecvTimeout) {
						if (!sock.recving) {
							auto * readingCo = sock.readingCo;
							sock.readingCo = nullptr;
							guard.unlock();

							if (readingCo)
								Scheduler::Instance().AddCoroutine(readingCo);
						}
						else {
							sock.wakeupRecv = true;

							guard.unlock();
						}


						if (Options::Instance().IsDebug()) {
							printf("recv [%d] wakeup recv when %s\n", sock.fd, sock.recving ? "recving" : "no recving");
						}
					}
				}
			}
		}
		
		//if (flag & EPOLLRDHUP) {
		//	Shutdown(evt->fd);
		//}
	}
	
	void NetEngine::CheckRecvTimeout() {
		int32_t i = 0;
		while (!_terminate) {
			int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			int32_t count = TIMEOUT_BATCH;
			while (i < _maxSocket && count > 0) {
				Socket& sock = _sockets[i];
				if (sock.fd != 0 && !sock.acceptor && sock.readingCo && !sock.closing && sock.recvTimeout > 0 && now > sock.recvTimeout) {
					Coroutine * co = nullptr;
					{
						std::unique_lock<spin_mutex> guard(sock.lock);
						if (sock.fd != 0 && !sock.acceptor && sock.readingCo && !sock.closing && sock.recvTimeout > 0 && now > sock.recvTimeout) {
							if (!sock.recving) {
								co = sock.readingCo;
								sock.readingCo = nullptr;
							}

							sock.recvTimeout = 0;
							sock.isRecvTimeout = true;
						}
					}

					if (co) 
						Scheduler::Instance().AddCoroutine(co);
				}
				
				++i;
				--count;
			}
			
			if (i >= _maxSocket)
				i = 0;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	int32_t NetEngine::AddToWorker(EpollEvent * evt) {
		EpollWorker * worker = nullptr;
		for (auto * w : _workers) {
			if (worker == nullptr || w->count < worker->count)
				worker = w;
		}

		if (worker) {
			int32_t flag = EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET;
			switch (evt->opt) {
			case EPOLL_OPT_ACCEPT: flag |= EPOLLIN; break;
			case EPOLL_OPT_CONNECT: flag |= EPOLLOUT; break;
			case EPOLL_OPT_IO: flag |= EPOLLIN | EPOLLOUT; break;
			}

			struct epoll_event ev;
			ev.data.ptr = evt;
			ev.events = flag;
			epoll_ctl(worker->epollFd, EPOLL_CTL_ADD, evt->sock, &ev);

			++worker->count;
			return worker->epollFd;
		}
		return -1;
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

					sock.readingCo = nullptr;
					sock.recvTimeout = 0;
					sock.isRecvTimeout = false;

					sock.sending = false;
					sock.closing = false;
					sock.waiting = false;

					sock.evt.opt = acceptor ? EPOLL_OPT_ACCEPT : EPOLL_OPT_IO;
					sock.evt.fd = fd;
					sock.evt.sock = s;

					sock.epollFd = AddToWorker((EpollEvent*)&sock.evt);

					return fd;
				}
			}
		}
		return -1;
	}

	void NetEngine::Close(Socket & sock) {
		epoll_ctl(sock.epollFd, EPOLL_CTL_DEL, sock.sock, nullptr);
		close(sock.sock);
		
		if (Options::Instance().IsDebug()) {
			printf("close [%d] sock\n", sock.fd);
		}

		sock.fd = 0;
		sock.sock = INVALID_SOCKET;
		sock.readingCo = nullptr;
		sock.recving = false;
		sock.wakeupRecv = false;
		sock.recvTimeout = 0;
		sock.isRecvTimeout = false;

		if (sock.sendBuf) {
			free(sock.sendBuf);
			sock.sendBuf = nullptr;
		}
	}

	void NetEngine::ShutdownListener(std::unique_lock<spin_mutex>& guard, Socket& sock) {
		epoll_ctl(sock.epollFd, EPOLL_CTL_DEL, sock.sock, nullptr);
		close(sock.sock);
		sock.fd = 0;
		sock.sock = INVALID_SOCKET;

		auto tmp = std::move(sock.waitAcceptCo);
		guard.unlock();

		for (auto * co : tmp)
			Scheduler::Instance().AddCoroutine(co);
	}
}
