#ifndef __UTIL_H__
#define __UTIL_H__
#include "hnet.h"
#include <chrono>

namespace zookeeper {
	inline int64_t GetTickCount() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	inline int64_t GetTimeStamp() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

class WaitCo {
public:
	WaitCo(hn_channel<int8_t, 1> ch) : _waitCh(ch) {}
	~WaitCo() {};

	inline void Wait() const {
		int8_t res;
		_waitCh >> res;
	}

private:
	hn_channel<int8_t, 1> _waitCh;
};

inline WaitCo DoWork(const std::function<void()>& f, int32_t size = 0) {
	hn_channel<int8_t, 1> ch;
	hn_fork [f, ch]() {
		f();
		ch << (int8_t)1;
	};
	return WaitCo(ch);
}

struct Server {
	int32_t id;
	std::string ip;
	int32_t electionPort;
	int32_t votePort;
};

#endif
