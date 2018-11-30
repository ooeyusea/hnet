#ifndef __OBJECT_H__
#define __OBJECT_H__
#include "ObjectProp.h"
#include "ObjectDescriptor.h"
#include "ObjectMemory.h"
#include "TableControl.h"
#include <map>
#include <list>
#include <vector>

class ObjectMgr;
class Object {
	friend class ObjectMgr;
	struct Callback {
		std::function<void(Object&, int32_t key, object_def::Prop& prop)> fn;
	};
public:
	Object(int32_t type, ObjectDescriptor& desc);
	~Object();

	int64_t GetID() const { return _objectId; }
	int32_t GetType() const { return _type; }

	template <typename T>
	inline typename object_def::FieldType<T>::result_type Get() {
		typedef typename object_def::FieldType<T>::result_type result_type;

		if (!_descriptor.Check<T>())
			return result_type();

		return _memory.Get<T>();
	};

	template <typename T>
	inline void Set(typename object_def::FieldType<T>::value_type t) {
		if (_descriptor.Check<T>()) {
			_memory.Set<T>(t);
			OnPropChange<T>();
		}
	}

	template <typename T>
	inline void Set(const typename object_def::FieldType<T>::value_type * t, int32_t size) {
		if (_descriptor.Check<T>()) {
			_memory.Set<T>(t, size);
			OnPropChange<T>();
		}
	}

	template <typename T>
	inline void SetString(const char * t) {
		if (_descriptor.Check<T>()) {
			_memory.SetString<T>(t);
		}
	}

	template <typename T>
	inline void RawSet(typename object_def::FieldType<T>::value_type t) {
		if (_descriptor.Check<T>()) {
			_memory.Set<T>(t);
		}
	}

	template <typename T>
	inline void RawSet(const typename object_def::FieldType<T>::value_type * t, int32_t size) {
		if (_descriptor.Check<T>()) {
			_memory.Set<T>(t, size);
			OnPropChange<T>();
		}
	}

	template <typename T>
	inline void RawSet(const char * t) {
		if (_descriptor.Check<T>()) {
			_memory.Set<T>(t);
		}
	}

	template <typename T>
	inline void RegisterPropChangeCB(const std::function<void(Object&, int32_t key, object_def::Prop& prop)>& fn) {
		_propChangeCallback[T::key].push_back({ fn });
	}

	inline void RegisterPropChangeCB(int32_t key, const std::function<void(Object&, int32_t key, object_def::Prop& prop)>& fn) {
		_propChangeCallback[key].push_back({ fn });
	}

	template <typename T>
	inline TableControl * GetTable() {
		if (!_descriptor.CheckTable<T>())
			return false;

		return &_tables[T::seq - 1];
	}

private:
	inline void SetID(int64_t id) { _objectId = id; }

	template <typename T>
	inline void OnPropChange() {
		auto itr = _propChangeCallback.find(T::key);
		if (itr != _propChangeCallback.end()) {
			for (const auto& cb : itr->second) {
				cb.fn(*this, T::key, _descriptor.Get<T>());
			}
		}
	}

private:
	int32_t _type;
	int64_t _objectId;
	Memory _memory;
	ObjectDescriptor& _descriptor;

	std::unordered_map<int32_t, std::list<Callback>> _propChangeCallback;

	std::vector<TableControl> _tables;
};

#endif //define __OBJECT_H__
