#ifndef __UTIL_H__
#define __UTIL_H__
#include "hnet.h"
#include <chrono>

namespace util {
	inline int64_t GetTickCount() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	inline int64_t GetTimeStamp() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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
		hn_fork[f, ch]() {
			f();
			ch << (int8_t)1;
		};
		return WaitCo(ch);
	}

	inline int64_t CalcUniqueId(const char * str) {
		int64_t hash = 0;
		const char * p = str;
		while (*p) {
			hash = hash * 131 + (*p);

			++p;
		}
		return hash;
	}

#define MAX_PATH_SIZE 260
}

#define SafeSprintf snprintf

#endif
