#include "net.h"
#include "coroutine.h"
#include "hnet.h"
#include "scheduler.h"
#include "options.h"

#define NET_INIT_FRAME 1024
#define MAX_NET_THREAD 4

namespace hyper_net {
	NetEngine::NetEngine() {
		_nextFd = 1;

		int32_t cpuCount = (int32_t)std::thread::hardware_concurrency();
		int32_t threadCount = (cpuCount * 2 > MAX_NET_THREAD) ? MAX_NET_THREAD : cpuCount * 2;
		for (int32_t i = 0; i < threadCount; ++i) {
			EpollWorker * worker = new EpollWorker;
			_workers.push_back(worker);

			worker->epollFd = epoll_create(1024);
			OASSERT(worker->epollFd > 0, "create epoll failed");

			std::thread([this, worker]() {
				ThreadProc(*worker);
			}).detach();
		}

		_terminate = false;
	}

	NetEngine::~NetEngine() {

	}

	int32_t NetEngine::Listen(const char * ip, const int32_t port) {
		socket_t sock = INVALID_SOCKET;
		if (INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
			return -1;
		}

		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
			close(sock);
			return -1;
		}

		int32_t reuse = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == -1) {
			close(sock);
			return -1;
		}

		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if ((addr.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE) {
			close(sock);
			return -1;
		}

		if (bind(sock, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0) {
			close(sock);
			return -1;
		}

		if (listen(sock, 128) < 0) {
			close(sock);
			return -1;
		}

		int32_t fd = Apply(sock, true);
		if (fd < 0) {
			close(sock);
			return -1;
		}

		return fd;
	}

	int32_t NetEngine::Connect(const char * ip, const int32_t port) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if ((addr.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE) {
			return -1;
		}

		socket_t sock = INVALID_SOCKET;
		if (INVALID_SOCKET == (sock = socket(AF_INET, SOCK_STREAM, 0))) {
			return -1;
		}

		if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
			close(sock);
			return -1;
		}

		long nonNegal = 1l;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
			close(sock);
			return -1;
		}

		int32_t res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
		if (res < 0 && errno != EINPROGRESS) {
			close(sock);
			return -1;
		}


		EpollConnector connector;
		connector.connect.opt = EPOLL_OPT_CONNECT;
		connector.connect.sock = sock;
		connector.co = co;

		int32_t error = 0;
		co->SetStatus(CoroutineState::CS_BLOCK);
		co->SetTemp(&error);

		AddToWorker((EpollEvent*)&connector);

		hn_yield;

		if (error == 0)
			return Apply(sock);

		close(sock);
		return -1;
	}

	int32_t NetEngine::Accept(int32_t fd) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		Socket& sock = _sockets[fd & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd && sock.acceptor) {
			socket_t comingSock = INVALID_SOCKET;
			if (!sock.waiting) {
				struct sockaddr_in addr;
				socklen_t len = sizeof(addr);

				comingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
				if (comingSock < 0) {
					if (errno == EAGAIN) {
						sock.waiting = true;
					}
					else
						return -1;
				}
				else {
					guard.unlock();

					if (fcntl(comingSock, F_SETFL, fcntl(comingSock, F_GETFD, 0) | O_NONBLOCK) == -1) {
						close(comingSock);
						return -1;
					}

					long nonNegal = 1l;
					if (setsockopt(comingSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
						close(comingSock);
						return -1;
					}

					return Apply(comingSock);
				}
			}

			sock.waitAcceptCo.push_back(co);
			co->SetStatus(CoroutineState::CS_BLOCK);
			co->SetTemp(&comingSock);
			guard.unlock();

			hn_yield;

			if (comingSock != INVALID_SOCKET) {
				if (fcntl(comingSock, F_SETFL, fcntl(comingSock, F_GETFD, 0) | O_NONBLOCK) == -1) {
					close(comingSock);
					return -1;
				}

				long nonNegal = 1l;
				if (setsockopt(comingSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nonNegal, sizeof(nonNegal)) == -1) {
					close(comingSock);
					return -1;
				}

				return Apply(comingSock);
			}

			return -1;
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
		};

		Socket& sock = _sockets[fd & MAX_SOCKET];
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
							sock.sendSize += size;
						}
						else {
							auto * readingCo = sock.readingCo;

							Close(sock);
							guard.unlock();

							if (readingCo)
								Scheduler::Instance().AddCoroutine(co);
						}
						break;
					}
					else
						offset += len;
				}
			}
		}
	}

	int32_t NetEngine::Recv(int32_t fd, char * buf, int32_t size) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must rune in coroutine");

		while (true) {
			Socket& sock = _sockets[fd & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == fd && !sock.acceptor && (sock.readingCo == nullptr || sock.readingCo == co) && !sock.closing) {
				int32_t len = recv(sock.sock, buf, size, 0);
				if (len <= 0) {
					if (errno == EAGAIN) {
						co->SetStatus(CoroutineState::CS_BLOCK);
						sock.readingCo = co;

						guard.unlock();
						hn_yield;
					}
					else {
						Close(sock);

						return len;
					}
				}
				else
					return len;
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
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				auto * readingCo = sock.readingCo;

				Close(sock);
				guard.unlock();

				if (readingCo)
					Scheduler::Instance().AddCoroutine(readingCo);
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	void NetEngine::Close(int32_t fd) {
		Socket& sock = _sockets[fd & MAX_SOCKET];
		std::unique_lock<spin_mutex> guard(sock.lock);
		if (sock.fd == fd) {
			if (!sock.acceptor) {
				if (!sock.closing) {
					sock.closing = true;
					if (!sock.sending) {
						auto * readingCo = sock.readingCo;

						Close(sock);
						guard.unlock();

						if (readingCo)
							Scheduler::Instance().AddCoroutine(readingCo);
					}
				}
			}
			else
				ShutdownListener(guard, sock);
		}
	}

	void NetEngine::ThreadProc(EpollWorker& worker) {
		epoll_event evs[Options::Instance().GetEpollEventSize()];
		while (!_terminate) {
			int32_t count = epoll_wait(worker.epollFd, evs, Options::Instance().GetEpollEventSize(), 1);
			if (count > 0) {
				for (int32_t i = 0; i < count; ++i) {
					EpollEvent * evt = (EpollEvent *) evs[i].data.ptr;
					switch (evt->opt) {
					case EPOLL_OPT_CONNECT: DealConnect(evt, evs[i].events); break;
					case EPOLL_OPT_ACCEPT: DealAccept(evt, evs[i].events); break;
					case EPOLL_OPT_IO: DealIO(evt, evs[i].events); break;
					}
				}
			}
			else if (count < 0) {

			}
		}
	}

	void NetEngine::DealConnect(EpollEvent * evt, int32_t flag) {
		EpollConnector * connector = (EpollConnector*)evt;
		if (flag & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			int32_t& error = *(int32_t*)connector->co->GetTemp();
			error = -1;
			Scheduler::Instance().AddCoroutine(connector->co);
		}
		else if (flag & EPOLLOUT) {
			Scheduler::Instance().AddCoroutine(connector->co);
		}
	}

	void NetEngine::DealAccept(EpollEvent * evt, int32_t flag) {
		if (flag & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			Shutdown(evt->fd);
		}
		else if (flag & EPOLLIN) {
			Socket& sock = _sockets[evt->fd & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock);
			if (sock.fd == evt->fd && sock.acceptor) {
				sock.waiting = false;

				struct sockaddr_in addr;
				socklen_t len = sizeof(addr);
				while (!sock.waitAcceptCo.empty()) {
					Coroutine * co = sock.waitAcceptCo.front();
					socket_t& commingSock = *(socket_t*)co->GetTemp();
					commingSock = accept(sock.sock, (struct sockaddr*)&addr, &len);
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
		if (flag & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			Shutdown(evt->fd);
		}
		else {
			bool valid = true;
			if (flag & EPOLLOUT) {
				Socket& sock = _sockets[evt->fd & MAX_SOCKET];
				std::unique_lock<spin_mutex> guard(sock.lock);
				if (sock.fd == evt->fd && !sock.acceptor) {
					int32_t offset = 0;
					while (offset < sock.sendSize) {
						int32_t len = send(sock.sock, sock.sendBuf + offset, sock.sendSize - offset, 0);
						if (len <= 0) {
							if (errno == EAGAIN) {
								sock.sending = true;

								memmove(sock.sendBuf, sock.sendBuf + offset, sock.sendSize - offset);
								sock.sendSize -= offset;
							}
							else {
								auto * readingCo = sock.readingCo;

								Close(sock);
								guard.unlock();

								if (readingCo)
									Scheduler::Instance().AddCoroutine(readingCo);
								valid = false;
							}
							break;
						}
					}
				}
			}

			if (valid && (flag & EPOLLIN)) {
				Socket& sock = _sockets[evt->fd & MAX_SOCKET];
				std::unique_lock<spin_mutex> guard(sock.lock);
				if (sock.fd == evt->fd && !sock.acceptor && !sock.closing) {
					auto * readingCo = sock.readingCo;
					sock.readingCo = nullptr;
					guard.unlock();

					if (readingCo)
						Scheduler::Instance().AddCoroutine(readingCo);
				}
			}
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

	int32_t NetEngine::Apply(socket_t s, bool acceptor) {
		int32_t count = MAX_SOCKET;
		while (count--) {
			int32_t fd = _nextFd;

			_nextFd++;
			if (_nextFd <= 0)
				_nextFd = 1;

			Socket& sock = _sockets[fd & MAX_SOCKET];
			std::unique_lock<spin_mutex> guard(sock.lock, std::defer_lock);
			if (guard.try_lock()) {
				if (sock.fd == 0) {
					sock.fd = fd;
					sock.sock = s;
					sock.acceptor = acceptor;

					sock.readingCo = nullptr;

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

		sock.fd = 0;
		sock.sock = INVALID_SOCKET;
		sock.readingCo = nullptr;

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
