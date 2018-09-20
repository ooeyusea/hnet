#include "timer_detail.h"
#include "scheduler.h"
#include "hnet.h"

namespace hyper_net {
	CountTicker::CountTicker(int64_t interval, int32_t count)
		: _last(this) {
		_impl = new CountTickerImpl(interval, count);
	}

	CountTicker::CountTicker(int64_t interval) 
		: _last(this) {
		_impl = new CountTickerImpl(interval, -1);
	}

	CountTicker::~CountTicker() {
		delete _impl;
	}

	int32_t CountTicker::Beat() {
		return _impl->Beat();
	}

	bool CountTicker::Check() {
		return _impl->Check();
	}

	void CountTicker::Stop() {
		_impl->Stop();
	}

	int32_t CountTicker::GetLastError() const {
		return _impl->GetLastError();
	}

	bool CountTickerImpl::CheckInner() {
		int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		if (now < _expire) {
			_co = Scheduler::Instance().CurrentCoroutine();
			TimerMgr::Instance().SleepUntil(_expire);
			_co = nullptr;
		}

		++_beated;
		if (_errno == 0 && (_count == -1 || _beated < _count + 1)) {
			_expire += _interval;
			return false;
		}

		return true;
	}

	void CountTickerImpl::Stop() {
		_errno = -1;

		if (_co)
			TimerMgr::Instance().Stop(_co);
	}
}

