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

		bool try_lock() {
			std::lock_guard<spin_mutex> lock(_mutex);
			if (!_locked) {
				_locked = true;
				return true;
			}
			return false;
		}

		void lock();
		void unlock();

		bool is_locked() {
			std::lock_guard<spin_mutex> lock(_mutex);
			return _locked;
		}

	private:
		spin_mutex _mutex;
		bool _locked = false;

		std::list<Coroutine*> _queue;
	};
}

#endif // !__COMUTEX_H__
