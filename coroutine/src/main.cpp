#include "scheduler.h"
#include "hnet.h"
#include "processer.h"
#include "net.h"
#include "timer.h"

#define MINUTE 60 * 1000

namespace hyper_net {
	Forker& Forker::operator-(const std::function<void()>& f) {
		hyper_net::Scheduler::Instance().AddCoroutine(f, stackSize);
		return *this;
	}

	Forker& Forker::operator-(int32_t size) {
		stackSize = size;
		return *this;
	}

	void Yielder::Do() {
		Processer::StaticCoYield();
	}

	int32_t NetAdapter::Connect(const char * ip, const int32_t port) {
		return NetEngine::Instance().Connect(ip, port);
	}

	int32_t NetAdapter::Listen(const char * ip, const int32_t port) {
		return NetEngine::Instance().Listen(ip, port);
	}

	int32_t NetAdapter::Accept(int32_t fd) {
		return NetEngine::Instance().Accept(fd);
	}

	int32_t NetAdapter::Recv(int32_t fd, char * buf, int32_t size) {
		return NetEngine::Instance().Recv(fd, buf, size);
	}

	void NetAdapter::Send(int32_t fd, const char * buf, int32_t size) {
		NetEngine::Instance().Send(fd, buf, size);
	}

	void NetAdapter::Close(int32_t fd) {
		NetEngine::Instance().Close(fd);
	}

	void NetAdapter::Shutdown(int32_t fd) {
		NetEngine::Instance().Shutdown(fd);
	}

	int32_t TimeAdapter::operator-(int64_t millisecond) {
		return TimerMgr::Instance().Sleep(millisecond);
	}

	int32_t TimeAdapter::operator+(int64_t timestamp) {
		while (true) {
			int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			if (now > timestamp) {
				int64_t diff = now - timestamp;
				if (diff > MINUTE)
					diff = MINUTE;

				TimerMgr::Instance().Sleep(diff);
			}
			else
				break;
		}
		return 0;
	}
}

int32_t main(int32_t argc, char ** argv) {
	return hyper_net::Scheduler::Instance().Start(argc, argv);
}
