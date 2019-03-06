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


	void CoSharedMutexImpl::lock() {
		std::unique_lock<spin_mutex> lock(_mutex);
		if (!_lockWrite && _lockRead == 0) {
			_lockWrite = true;
			return;
		}

		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		co->SetStatus(CoroutineState::CS_BLOCK);
		_writeQueue.push_back(co);
		lock.unlock();

		hn_yield;
	}

	void CoSharedMutexImpl::unlock() {
		std::unique_lock<spin_mutex> lock(_mutex);
		OASSERT(_lockWrite, "mutex is not locked");

		if (!_writeQueue.empty()) {
			Coroutine * co = _writeQueue.front();
			_writeQueue.pop_front();

			Scheduler::Instance().AddCoroutine(co);
		}
		else {
			_lockWrite = false;

			if (!_readQueue.empty()) {
				_lockRead = (int32_t)_readQueue.size();
				
				for (auto co : _readQueue)
					Scheduler::Instance().AddCoroutine(co);
				
				_readQueue.clear();
			}
		}
	}

	void CoSharedMutexImpl::lock_shared() {
		std::unique_lock<spin_mutex> lock(_mutex);
		if (!_lockWrite) {
			if (!_writeFirst || _writeQueue.empty()) {
				_lockRead++;
				return;
			}
		}

		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		co->SetStatus(CoroutineState::CS_BLOCK);
		_readQueue.push_back(co);
		lock.unlock();

		hn_yield;
	}

	void CoSharedMutexImpl::unlock_shared() {
		std::unique_lock<spin_mutex> lock(_mutex);
		OASSERT(_lockRead > 0, "mutex is not locked");
		--_lockRead;

		if (_lockRead == 0) {
			if (!_writeQueue.empty()) {
				_lockWrite = true;

				Coroutine * co = _writeQueue.front();
				_writeQueue.pop_front();

				Scheduler::Instance().AddCoroutine(co);
			}
		}
	}

	void CoSharedMutexImpl::unlock_and_lock_shared() {
		std::unique_lock<spin_mutex> lock(_mutex);
		OASSERT(_lockWrite, "mutex is not locked");

		if (!_writeQueue.empty()) {
			Coroutine * co = _writeQueue.front();
			_writeQueue.pop_front();

			Scheduler::Instance().AddCoroutine(co);

			co = Scheduler::Instance().CurrentCoroutine();
			co->SetStatus(CoroutineState::CS_BLOCK);
			_readQueue.push_back(co);
			lock.unlock();

			hn_yield;
		}
		else {
			_lockWrite = false;
			++_lockRead;

			if (!_readQueue.empty()) {
				_lockRead += (int32_t)_readQueue.size();

				for (auto co : _readQueue)
					Scheduler::Instance().AddCoroutine(co);

				_readQueue.clear();
			}
		}
	}

	void CoSharedMutexImpl::unlock_shared_and_lock() {
		std::unique_lock<spin_mutex> lock(_mutex);
		OASSERT(_lockRead > 0, "mutex is not locked");
		--_lockRead;

		if (_lockRead == 0)
			_lockWrite = true;
		else {
			Coroutine * co = Scheduler::Instance().CurrentCoroutine();
			co->SetStatus(CoroutineState::CS_BLOCK);
			_writeQueue.push_back(co);
			lock.unlock();

			hn_yield;
		}
	}

	CoSharedMutex::CoSharedMutex(bool writeFirst) {
		_impl = new CoSharedMutexImpl(writeFirst);
	}

	CoSharedMutex::~CoSharedMutex() {
		delete _impl;
	}

	bool CoSharedMutex::try_lock() {
		return _impl->try_lock();
	}

	void CoSharedMutex::lock() {
		_impl->lock();
	}

	void CoSharedMutex::unlock() {
		_impl->unlock();
	}

	bool CoSharedMutex::is_locked() {
		return _impl->is_locked();
	}

	bool CoSharedMutex::try_lock_shared() {
		return _impl->try_lock_shared();
	}

	void CoSharedMutex::lock_shared() {
		_impl->lock_shared();
	}

	void CoSharedMutex::unlock_shared() {
		_impl->unlock_shared();
	}

	void CoSharedMutex::unlock_and_lock_shared() {
		_impl->unlock_and_lock_shared();
	}

	void CoSharedMutex::unlock_shared_and_lock() {
		_impl->unlock_shared_and_lock();
	}
}