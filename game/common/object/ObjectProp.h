/* 
 * File:   IObjectMgr.h
 * Author: ooeyusea
 *
 * Created on March 3, 2015, 10:46 AM
 */

#ifndef __IOBJECTMGR_H__
#define __IOBJECTMGR_H__
#include "util.h"

namespace object_def {
	template <typename T>
	struct Space;

	template <>
	struct Space<char> {
		static const int32_t size = sizeof(char);
	};

	template <>
	struct Space<int8_t> {
		static const int32_t size = sizeof(int8_t);
	};

	template <>
	struct Space<uint8_t> {
		static const int32_t size = sizeof(uint8_t);
	};

	template <>
	struct Space<int16_t> {
		static const int32_t size = sizeof(int16_t);
	};

	template <>
	struct Space<uint16_t> {
		static const int32_t size = sizeof(uint16_t);
	};

	template <>
	struct Space<int32_t> {
		static const int32_t size = sizeof(int32_t);
	};

	template <>
	struct Space<uint32_t> {
		static const int32_t size = sizeof(uint32_t);
	};

	template <>
	struct Space<int64_t> {
		static const int32_t size = sizeof(int64_t);
	};

	template <>
	struct Space<uint64_t> {
		static const int32_t size = sizeof(uint64_t);
	};

	template <>
	struct Space<float> {
		static const int32_t size = sizeof(float);
	};

	template <>
	struct Space<double> {
		static const int32_t size = sizeof(double);
	};

	template <typename T, int32_t len>
	struct Space<T[len]> {
		static const int32_t size = len * Space<T>::size;
	};

	template <typename... Args>
	struct Setting {};

	template <>
	struct Setting<> {
		static const int64_t flag = 0;
	};

	template <typename T>
	struct Setting<T> {
		static const int64_t flag = T::flag;
	};

	template <typename T, typename... Args>
	struct Setting<T, Args...> {
		static const int64_t flag = T::flag | Attribute<Args...>::value;
	};

	template <typename T, int32_t start, typename... Args>
	struct FiledStart;

	template <typename T, int32_t start>
	struct FiledStart<T, start> {
		static const int32_t index = start;
		static const int32_t offset = 0;
		static const int32_t space = 0;
		static const int32_t seq = 0;
	};

	template <typename T, int32_t start, typename Before>
	struct FiledStart<T, start, Before> {
		static const int32_t index = start;
		static const int32_t offset = Before::space;
		static const int32_t space = Before::space;
		static const int32_t seq = Before::seq;
	};

	template <int32_t start, int32_t k, typename T, typename Before, typename... Args>
	struct Field {
		static const int32_t key = k;
		static const int32_t index = (start == 0 ? Before::index : start);
		static const int32_t offset = Before::space;
		static const int32_t space = Before::space + Space<T>::size;
		static const int32_t size = Space<T>::size;
		static const int64_t setting = Setting<Args...>::flag;
		static const int32_t seq = Before::seq + 1;
	};

	template<typename T, typename... Args>
	struct TableStart;

	template <typename T>
	struct TableStart<T> {
		static const int32_t seq = 0;
	};

	template <typename T, typename Before>
	struct TableStart<T, Before> {
		static const int32_t seq = Before::seq;
	};

	template <typename Test, typename... Args>
	struct In;

	template <typename Test>
	struct In<Test> {
		static const bool value = false;
	};

	template <typename Test, typename... Args>
	struct In<Test, Test, Args...> {
		static const bool value = true;
	};

	template <typename Test, typename T, typename... Args>
	struct In<Test, T, Args...> {
		static const bool value = In<Test, Args...>::value;
	};

	template <typename Attr, typename T>
	struct Is;

	template <int32_t s, int32_t k, typename Attr, typename T, typename Before, typename... Args>
	struct Is<Attr, Field<s, k, T, Before, Args...>> {
		static const bool value = In<Attr, Args...>::value;
	};

	constexpr int64_t CalcUniqueId(int64_t hash, const char * str) {
		return *str ? CalcUniqueId(((hash * 131) + *str) % 4294967295, str + 1) : hash;
	}

	struct Prop {
		const int32_t key;
		const int32_t index;
		const int32_t offset;
		const int32_t space;
		const int32_t size;
		const int64_t setting;
	};

	template <typename T>
	struct FieldType;

	template <int32_t s, int32_t k, typename T, typename Before, typename... Args>
	struct FieldType<Field<s, k, T, Before, Args...>>{
		typedef T value_type;
		typedef T result_type;
	};

	template <int32_t s, int32_t k, typename T, int32_t len, typename Before, typename... Args>
	struct FieldType<Field<s, k, T[len], Before, Args...>> {
		typedef T value_type;
		typedef const T * result_type;
	};

	struct PtrTraits {};
	struct NoPtrTraits {};

	template <typename T>
	struct TypeTraits {
		typedef NoPtrTraits traits_type;
	};

	template <typename T, int32_t len>
	struct TypeTraits<T[len]> {
		typedef PtrTraits traits_type;
	};

	template <typename T>
	struct FieldTraits;

	template <int32_t s, int32_t k, typename T, typename Before, typename... Args>
	struct FieldTraits<Field<s, k, T, Before, Args...>> {
		typedef typename TypeTraits<T>::traits_type traits_type;
	};

	struct key {
		static const int64_t flag = 1;
	};

	struct nokey {
		static const int64_t flag = 1;
	};

	struct IntKey {};
	struct NoKey {};
	struct StringKey {};

	template <typename T>
	struct KeyType;

	template <>
	struct KeyType<int8_t> {
		typedef IntKey type;
	};

	template <>
	struct KeyType<int16_t> {
		typedef IntKey type;
	};

	template <>
	struct KeyType<int32_t> {
		typedef IntKey type;
	};

	template <>
	struct KeyType<int64_t> {
		typedef IntKey type;
	};

	template <int32_t len>
	struct KeyType<char [len]> {
		typedef StringKey type;
	};

	template <typename T>
	struct TableKeyTraits;

	template<typename T, int32_t start, typename... Args>
	struct TableKeyTraits<FiledStart<T, start, Args...>> {
		typedef NoKey type;
	};

	template<int32_t s, int32_t k, typename T, typename Before, typename... Args>
	struct TableKeyTraits<Field<s, k, T, Before, Args...>> {
		typedef typename KeyType<T>::type type;
	};

	template <bool t, typename T>
	struct TwoKeyCheck {
		static const bool value = false;
	};

	template <typename T>
	struct TwoKeyCheck<false, T> {
		static const bool value = false;
	};

	template <typename T>
	struct TwoKeyCheck<true, T> {
		static const bool value = true;
	};

	template <typename T, int32_t start, typename... Args>
	struct TwoKeyCheck<true, FiledStart<T, start, Args...>> {
		static const bool value = false;
	};

	template <typename T>
	struct KeyFinder;

	template<typename T, int32_t start, typename... Args>
	struct KeyFinder<FiledStart<T, start, Args...>> {
		typedef FiledStart<T, start, Args...> type;
	};

	template<int32_t s, int32_t k, typename T, typename Before, typename... Args>
	struct KeyFinder<Field<s, k, T, Before, Args...>> {
		typedef typename KeyFinder<Before>::type before_key;
		typedef typename std::conditional<In<key, Args...>::value, Field<s, k, T, Before, Args...>, before_key>::type type;

		static_assert(!TwoKeyCheck<In<key, Args...>::value, before_key>::value, "this is two key in this table");
	};
}

#define BEGIN_CLASS(name, start)\
struct name {\
	static const int32_t type = (int32_t)object_def::CalcUniqueId(0, #name);\
	typedef object_def::FiledStart<name, start> __start;\
	typedef object_def::TableStart<name> __tableStart;

#define BEGIN_CLASS_INHERIT(name, start, before)\
struct name {\
	static const int32_t type = (int32_t)object_def::CalcUniqueId(0, #name); \
	typedef object_def::FiledStart<name, start, typename before::__end> __start;\
	typedef object_def::TableStart<name, typename before::__tableEnd> __tableStart;

#define END_CLASS()\
}

#define DECL_PROP(name, start, type)\
	typedef object_def::Field<start, (int32_t)object_def::CalcUniqueId(0, #name), type, __start> name;

#define DECL_PROP_AFTER(name, start, type, attr)\
	typedef object_def::Field<start, (int32_t)object_def::CalcUniqueId(0, #name), type, attr> name;

#define DECL_PROP_ATTR(name, start, type, ...)\
	typedef object_def::Field<start, (int32_t)object_def::CalcUniqueId(0, #name), type, __start, ##__VA_ARGS__> name;

#define DECL_PROP_ATTR_AFTER(name, start, type, attr, ...)\
	typedef object_def::Field<start, (int32_t)object_def::CalcUniqueId(0, #name), type, attr, ##__VA_ARGS__> name;

#define PROP_COMIT()\
	typedef object_def::Field<0, 0, int8_t, __start> __end;\

#define PROP_COMIT_AFTER(attr)\
	typedef attr __end;

#define BEGIN_TABLE(name)\
struct name{\
	typedef __tableStart before_type;\
	static const int32_t seq = __tableStart::seq + 1;\
	static const int32_t type = (int32_t)object_def::CalcUniqueId(0, #name);\
	typedef object_def::FiledStart<name, 0> __start;

#define BEGIN_TABLE_AFTER(name, before)\
struct name{\
	typedef before before_type;\
	static const int32_t seq = before::seq + 1;\
	static const int32_t type = (int32_t)object_def::CalcUniqueId(0, #name);\
	typedef object_def::FiledStart<name, 0> __start;

#define DECL_COLUMN(name, type, key)\
	typedef object_def::Field<0, object_def::CalcUniqueId(0, #name), type, __start, object_def::##key> name;

#define DECL_COLUMN_AFTER(name, type, key, attr)\
	typedef object_def::Field<0, object_def::CalcUniqueId(0, #name), type, attr, object_def::##key> name;

#define END_TABLE(attr)\
	typedef attr __end;\
	typedef typename object_def::KeyFinder<__end>::type key_type;\
 }

#define TABLE_COMIT_AFTER(attr)\
	typedef attr __tableEnd;

#define TABLE_COMIT()\
	typedef __tableStart __tableEnd;

#define DECL_ATTR(name)\
struct name {\
	static const int64_t flag = 1;\
};

#define DECL_ATTR_AFTER(name, before)\
struct name {\
	static const int64_t flag = (before::flag << 1);\
};

#endif //define __IOBJECTMGR_h__
