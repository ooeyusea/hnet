#ifndef __OBJECT_MEMORY_H__
#define __OBJECT_MEMORY_H__
#include "util.h"
#include "ObjectProp.h"

class Memory {
public:
	Memory(int32_t space) {
		_data = (char*)malloc(space);
		_space = space;
	}

	~Memory() {
		free(_data);
	}

	void Reset() {
		memset(_data, 0, _space);
	}

	template <typename T>
	inline typename object_def::FieldType<T>::result_type Get() {
		typedef typename object_def::FieldTraits<T>::traits_type traits_type;
		return Get<T>(traits_type());
	};

	template <typename T>
	inline void Set(typename object_def::FieldType<T>::value_type t) {
		typedef typename object_def::FieldTraits<T>::traits_type traits_type;
		Set<T>(t, traits_type());
	}

	template <typename T>
	inline void Set(const typename object_def::FieldType<T>::value_type * t, int32_t size) {
		typedef typename object_def::FieldTraits<T>::traits_type traits_type;
		Set<T>(t, traits_type());
	}

	template <typename T>
	inline void SetString(const char * t) {
		typedef typename object_def::FieldType<T>::value_type value_type;
		static_assert(std::is_same<value_type, char>::value, "only string type can use this function");

		int32_t size = (int32_t)strlen(t);
		size = (size >= T::size ? T::size - 1 : size);
		memcpy(_data + T::offset, t, size);
		*(_data + T::offset + size) = 0;
	}

private:
	template <typename T>
	inline typename object_def::FieldType<T>::value_type Get(object_def::NoPtrTraits) {
		typedef typename object_def::FieldType<T>::value_type RstType;
		return *(RstType*)(_data + T::offset);
	};

	template <typename T>
	inline const typename object_def::FieldType<T>::value_type * Get(object_def::PtrTraits) {
		typedef typename object_def::FieldType<T>::value_type RstType;
		return (RstType*)(_data + T::offset);
	};

	template <typename T>
	inline void Set(typename object_def::FieldType<T>::value_type t, object_def::NoPtrTraits) {
		typedef typename object_def::FieldType<T>::value_type RstType;
		*(RstType*)(_data + T::offset) = t;
	}

	template <typename T>
	inline void Set(const typename object_def::FieldType<T>::value_type * t, int32_t size, object_def::PtrTraits) {
		size = (size > T::size ? T::size : size);
		memcpy(_data + T::offset, t, size * sizeof(typename object_def::FieldType<T>::value_type));
	}

private:
	char * _data = nullptr;
	int32_t _space = 0;
};

#endif //defined __OBJECT_MEMORY_H__
