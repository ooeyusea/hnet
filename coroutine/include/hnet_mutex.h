#ifndef __HNET_MUTEX_H__
#define __HNET_MUTEX_H__
#include <mutex>

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

	class CoSharedMutexImpl;
	class CoSharedMutex {
	public:
		CoSharedMutex(bool writeFirst);
		~CoSharedMutex();

		bool try_lock();
		void lock();
		void unlock();
		bool is_locked();

		bool try_lock_shared();
		void lock_shared();
		void unlock_shared();

		void unlock_and_lock_shared();
		void unlock_shared_and_lock();

	private:
		CoSharedMutexImpl * _impl;
	};

	template<typename SharedMutex>
	class shared_lock_guard {
	private:
		SharedMutex& m;

	public:
		typedef SharedMutex mutex_type;
		shared_lock_guard(shared_lock_guard const&) = delete;
		shared_lock_guard& operator=(shared_lock_guard const&) = delete;

		explicit shared_lock_guard(SharedMutex& m_) :
			m(m_) {
			m.lock_shared();
		}

		shared_lock_guard(SharedMutex& m_, std::adopt_lock_t) :
			m(m_) {
		}

		~shared_lock_guard() {
			m.unlock_shared();
		}
	};

	template<typename Mutex>
	class shared_lock
	{
	protected:
		Mutex* m;
		bool is_locked;

	public:
		typedef Mutex mutex_type;
		shared_lock(shared_lock const&) = delete;
		shared_lock& operator=(shared_lock const&) = delete;

		shared_lock() noexcept
			: m(0)
			, is_locked(false) {
		}

		explicit shared_lock(Mutex& m_) 
			: m(&m_)
			, is_locked(false) {
			lock();
		}

		shared_lock(Mutex& m_, std::adopt_lock_t) 
			: m(&m_)
			, is_locked(true) {
		}

		shared_lock(Mutex& m_, std::defer_lock_t) noexcept
			: m(&m_)
			, is_locked(false) {
		}

		shared_lock(Mutex& m_, std::try_to_lock_t)
			: m(&m_)
			, is_locked(false) {
			try_lock();
		}

		shared_lock(shared_lock<Mutex>&& other) noexcept
			: m(other.m)
			, is_locked(other.is_locked)
		{
			other.is_locked = false;
			other.m = 0;
		}

		explicit shared_lock(std::unique_lock<Mutex> && other) 
			: m(other.m)
			, is_locked(other.is_locked) {

			if (is_locked) {
				m->unlock_and_lock_shared();
			}

			other.is_locked = false;
			other.m = 0;
		}

		shared_lock& operator=(shared_lock<Mutex> && other)  {
			shared_lock temp(std::move(other));
			swap(temp);
			return *this;
		}

		shared_lock& operator=(std::unique_lock<Mutex>&& other) {
			shared_lock temp(std::move(other));
			swap(temp);
			return *this;
		}

		void swap(shared_lock& other) noexcept {
			std::swap(m, other.m);
			std::swap(is_locked, other.is_locked);
		}

		Mutex* mutex() const noexcept {
			return m;
		}

		Mutex* release() noexcept {
			Mutex* const res = m;
			m = 0;
			is_locked = false;
			return res;
		}

		~shared_lock() {
			if (owns_lock()) {
				m->unlock_shared();
			}
		}

		void lock() {
			if (m == 0) {
				throw std::logic_error("shared_lock has no mutex");
			}

			if (owns_lock()){
				throw std::logic_error("shared_lock owns already the mutex");
			}

			m->lock_shared();
			is_locked = true;
		}

		bool try_lock() {
			if (m == 0) {
				throw std::logic_error("shared_lock has no mutex");
			}

			if (owns_lock()) {
				throw std::logic_error("shared_lock owns already the mutex");
			}

			is_locked = m->try_lock_shared();
			return is_locked;
		}

		void unlock() {
			if (m == 0) {
				throw std::logic_error("shared_lock has no mutex");
			}

			if (!owns_lock()) {
				throw std::logic_error("shared_lock owns doesn't own the mutex");
			}

			m->unlock_shared();
			is_locked = false;
		}

		explicit operator bool() const noexcept {
			return owns_lock();
		}

		bool owns_lock() const noexcept {
			return is_locked;
		}

		std::unique_lock<Mutex> upgrade() {
			if (m == 0) {
				throw std::logic_error("shared_lock has no mutex");
			}

			if (owns_lock()) {
				throw std::logic_error("shared_lock owns already the mutex");
			}

			return 
		}
	};

	template<typename Mutex>
	void swap(shared_lock<Mutex>& lhs, shared_lock<Mutex>& rhs) noexcept {
		lhs.swap(rhs);
	}
}

#define hn_mutex hyper_net::CoMutex
#define hn_shared_mutex hyper_net::CoSharedMutex
#define hn_shared_lock hyper_net::shared_lock
#define hn_shared_lock_guard hyper_net::shared_lock_guard

#endif // !__HNET_MUTEX_H__
