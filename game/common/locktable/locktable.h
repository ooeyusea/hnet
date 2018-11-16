#ifndef __LOCKTABLE_H__
#define __LOCKTABLE_H__
#include "hnet.h"
#include <unordered_map>
#include <memory>
#include <mutex>

template<typename Key, typename T, typename Mutex>
class LockUnit {
public:
	class Locker {
	public:
		explicit Locker(std::shared_ptr<LockUnit>& ptr) : _ptr(ptr), _owner(false) {
			_ptr->lock();
			_owner = true;
		};

		~Locker() {
			_owner = false;
			_ptr->unlock();
		}

		inline void lock() {
			if (!_owner) {
				_ptr->lock();
				_owner = true;
			}
		}

		inline void unlock() {
			if (_owner) {
				_owner = false;
				_ptr->unlock();
			}
		}

	private:
		std::shared_ptr<LockUnit>& _ptr;
		bool _owner;
	};

public:
	template <typename... Args>
	LockUnit(const Key& k, Args... args) {
		_key = k;
		_t = new T(args...);
	};

	~LockUnit() {
		if (_t) {
			delete _t;
			_t = nullptr;
		}
	}

	T * Get() const { return _t; }
	const Key& GetKey() const { return _key; }

	inline void lock() {
		_lock.lock();
	}

	inline bool try_lock() {
		return _lock.try_lock();
	}

	inline void unlock() {
		_lock.unlock();
	}

	inline void Release() {
		if (_t) {
			delete _t;
			_t = nullptr;
		}
	}

private:
	Key _key;
	T * _t;
	Mutex _lock;
};

template <typename Key, typename T, typename Mutex, typename Mutex2 = hn_mutex>
class LockTable {
public:
	typedef LockUnit<Key, T, Mutex2> UnitType;

	LockTable() {}
	~LockTable() {}

	template <typename... Args>
	std::shared_ptr<UnitType> FindCreate(const Key& k, Args... args) {
		std::lock_guard<Mutex> lock(_mutex);
		auto itr = _tables.find(k);
		if (itr == _tables.end()) {
			auto ptr = std::make_shared<UnitType>(k, args...);
			auto ret = _tables.insert(std::make_pair(k, ptr));
			itr = ret.first;
		}
		
		return itr->second;
	}

	std::shared_ptr<UnitType> Find(const Key& k) {
		std::lock_guard<Mutex> lock(_mutex);
		auto itr = _tables.find(k);
		if (itr != _tables.end())
			return itr->second;
		
		return nullptr;
	}

	inline void Remove(const Key& k, const std::function<void()> & fn = nullptr) {
		std::unique_lock<Mutex> lock(_mutex);
		_tables.erase(k);
		if (fn)
			fn();
	}

private:
	Mutex _mutex;
	std::unordered_map<Key, std::shared_ptr<UnitType>> _tables;
};

#endif // !__LOCKTABLE_H__
