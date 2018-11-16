#ifndef __SPIN_MUTEX_H__
#define __SPIN_MUTEX_H__
#include <atomic>
#include <shared_mutex>

class spin_mutex {
	std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
	spin_mutex() = default;
	spin_mutex(const spin_mutex&) = delete;
	spin_mutex& operator= (const spin_mutex&) = delete;

	bool try_lock() {
		return !flag.test_and_set(std::memory_order_acquire);
	}

	void lock() {
		while (flag.test_and_set(std::memory_order_acquire))
			;
	}

	void unlock() {
		flag.clear(std::memory_order_release);
	}
};

class fake_mutex {
public:
	fake_mutex() = default;
	fake_mutex(const fake_mutex&) = delete;
	fake_mutex& operator= (const fake_mutex&) = delete;

	inline bool try_lock() { return true; }

	inline void lock() {}

	inline void unlock() {}
};

class spin_rw_mutex {
	std::atomic<int> flag;
public:
	spin_rw_mutex() = default;
	spin_rw_mutex(const spin_rw_mutex&) = delete;
	spin_rw_mutex& operator= (const spin_rw_mutex&) = delete;

	void lock() {
		int expect = 0;
		do {
			expect = 0;
		} while (flag.compare_exchange_weak(expect, -1, std::memory_order_acquire));
	}

	void unlock() {
		flag.store(0, std::memory_order_release);
	}

	void lock_shared() {
		int expect = 0;
		do {
			if (expect < 0)
				expect = 0;
		} while (flag.compare_exchange_weak(expect, expect + 1, std::memory_order_acquire));
	}

	void unlock_shared() {
		flag.fetch_sub(1, std::memory_order_release);
	}
};

#endif //__SPIN_MUTEX_H__
