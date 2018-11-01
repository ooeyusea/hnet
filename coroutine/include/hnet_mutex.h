#ifndef __HNET_MUTEX_H__
#define __HNET_MUTEX_H__

namespace hyper_net {
	class CoMutexImpl;
	class CoMutex {
	public:
		CoMutex();
		~CoMutex();

		bool try_lock();
		void lock();
		void unlock();
		bool is_locked();

	private:
		CoMutexImpl * _impl;
	};
}

#define hn_mutex hyper_net::CoMutex

#endif // !__HNET_MUTEX_H__
