#ifndef __COMUTEX_H__
#define __COMUTEX_H__
#include "util.h"
#include "spin_mutex.h"
#include "coroutine.h"

namespace hyper_net {
	class CoMutexImpl {
	public:
		CoMutexImpl() {}
		~CoMutexImpl() {}

		inline bool try_lock() {
			std::lock_guard<spin_mutex> lock(_mutex);
			if (!_locked) {
				_locked = true;
				return true;
			}
			return false;
		}

		void lock();
		void unlock();

		inline bool is_locked() {
			std::lock_guard<spin_mutex> lock(_mutex);
			return _locked;
		}

	private:
		spin_mutex _mutex;
		bool _locked = false;

		std::list<Coroutine*> _queue;
	};

	class CoSharedMutexImpl {
	public:
		CoSharedMutexImpl(bool writeFirst) : _writeFirst(writeFirst){}
		~CoSharedMutexImpl() {}

		bool try_lock() {
			std::lock_guard<spin_mutex> lock(_mutex);
			if (!_lockWrite && _lockRead == 0) {
				_lockWrite = true;
				return true;
			}
			return false;
		}

		void lock();
		void unlock();

		bool is_locked() {
			std::lock_guard<spin_mutex> lock(_mutex);
			return _lockWrite;
		}

		inline bool try_lock_shared() {
			std::lock_guard<spin_mutex> lock(_mutex);
			if (!_lockWrite) {
				if (!_writeFirst || _writeQueue.empty()) {
					_lockRead++;
					return true;
				}
			}
			return false;
		}

		void lock_shared();
		void unlock_shared();

		void unlock_and_lock_shared();
		void unlock_shared_and_lock();

	private:
		spin_mutex _mutex;
		bool _lockWrite = false;
		int32_t _lockRead = 0;
		bool _writeFirst = false;

		std::list<Coroutine*> _readQueue;
		std::list<Coroutine*> _writeQueue;
	};
}

#endif // !__COMUTEX_H__
