#include "scheduler.h"
#include "hnet.h"
#include "processer.h"
#include "net.h"
#include "timer.h"
#include <sstream>
#include <iomanip>
#ifndef WIN32
#include <signal.h>
#endif

#define MINUTE (60 * 1000)

namespace hyper_net {
	Forker& Forker::operator-(const std::function<void()>& f) {
		if (stackSize == 0)
			stackSize = Options::Instance().GetDefaultStackSize();
		hyper_net::Scheduler::Instance().AddCoroutine(f, stackSize);
		return *this;
	}

	Forker& Forker::operator-(int32_t size) {
		if (size == 0) 
			stackSize = Options::Instance().GetDefaultStackSize();
		else if (size < Options::Instance().GetMinStackSize())
			size = Options::Instance().GetMinStackSize();
		stackSize = size;
		return *this;
	}

	void Yielder::Do() {
		Processer::StaticCoYield();
	}

	int32_t NetAdapter::Connect(const char * ip, const int32_t port, int32_t proto) {
		return NetEngine::Instance().Connect(ip, port, proto);
	}

	int32_t NetAdapter::Listen(const char * ip, const int32_t port, int32_t proto) {
		return NetEngine::Instance().Listen(ip, port, proto);
	}

	int32_t NetAdapter::Accept(int32_t fd, char * remoteIp, int32_t remoteIpSize, int32_t * remotePort) {
		return NetEngine::Instance().Accept(fd, remoteIp, remoteIpSize, remotePort);
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

	bool NetAdapter::Test(int32_t fd) {
		return NetEngine::Instance().Test(fd);
	}

	int32_t TimeAdapter::operator-(int64_t millisecond) {
		return TimerMgr::Instance().Sleep(millisecond);
	}

	int32_t TimeAdapter::operator+(int64_t timestamp) {
		while (true) {
			int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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

	int32_t TimeAdapter::operator+(const char * date) {
		std::tm t = {};
		std::istringstream ss(date);
		ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");

		int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::from_time_t(std::mktime(&t)).time_since_epoch()).count();
		return operator+(timestamp);
	}
}

int32_t main(int32_t argc, char ** argv) {
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	return hyper_net::Scheduler::Instance().Start(argc, argv);
}
