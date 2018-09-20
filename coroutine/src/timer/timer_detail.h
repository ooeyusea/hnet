#ifndef __TIMER_DETAIL_H__
#define __TIMER_DETAIL_H__
#include "timer.h"

namespace hyper_net {
	class Ticker {
	public:
		Ticker(int64_t interval) : _interval(interval), _started(false) {}
		~Ticker() {}

		inline bool Check() {
			if (!_started) {
				_expire = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + _interval;
				_started = true;
			}

			return CheckInner();
		}

	protected:
		virtual bool CheckInner() = 0;

	protected:
		int64_t _interval;
		int64_t _expire;
		bool _started;
	};

	class CountTickerImpl : public Ticker {
	public:
		CountTickerImpl(int64_t interval, int32_t count) : Ticker(interval), _count(count) {}
		~CountTickerImpl() {}

		virtual bool CheckInner();

		void Stop();
		int32_t GetLastError() const { return _errno; }
		int32_t Beat() const { return _beated; }

	private:
		int32_t _count;
		int32_t _beated = 0;

		int32_t _errno = 0;

		Coroutine * _co = nullptr;
	};
}

#endif // !__TIMER_DETAIL_H__
