#include "comutex.h"
#include "scheduler.h"
#include "hnet.h"

namespace hyper_net {
	void CoMutexImpl::lock() {
		std::unique_lock<spin_mutex> lock(_mutex);
		if (!_locked) {
			_locked = true;
			return;
		}

		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		co->SetStatus(CoroutineState::CS_BLOCK);
		_queue.push_back(co);
		lock.unlock();
		
		hn_yield;
	}

	void CoMutexImpl::unlock() {
		std::unique_lock<spin_mutex> lock(_mutex);
		OASSERT(_locked, "mutex is not locked");

		if (!_queue.empty()) {
			Coroutine * co = _queue.front();
			_queue.pop_front();

			Scheduler::Instance().AddCoroutine(co);
		}
		else
			_locked = false;
	}

	CoMutex::CoMutex() {
		_impl = new CoMutexImpl;
	}
	
	CoMutex::~CoMutex() {
		delete _impl;
	}

	bool CoMutex::try_lock() {
		return _impl->try_lock();
	}

	void CoMutex::lock() {
		_impl->lock();
	}

	void CoMutex::unlock() {
		_impl->unlock();
	}

	bool CoMutex::is_locked() {
		return _impl->is_locked();
	}
}