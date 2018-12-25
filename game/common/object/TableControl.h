#ifndef __TABLECONTROL_H__
#define __TABLECONTROL_H__
#include "ObjectProp.h"
#include "ObjectDescriptor.h"
#include "ObjectMemory.h"
#include <vector>
#include <string>
#include <unordered_map>

class TableControl;
class TableRow {
public:
	TableRow(TableControl& table, TableDescriptor& desc) : _table(table), _memory(desc.GetSpace()), _descriptor(desc) {}
	~TableRow() {}

	template <typename T>
	inline typename object_def::FieldType<T>::result_type Get() {
		typedef typename object_def::FieldType<T>::result_type result_type;

		if (!_descriptor.Check<T>())
			return result_type();

		return _memory.Get<T>();
	};

	template <typename T>
	inline void Set(typename object_def::FieldType<T>::value_type t) {
		typedef typename object_def::FieldType<T>::value_type value_type;

		if (_descriptor.Check<T>()) {
			if (object_def::Is<object_def::key, T>::value) {
				value_type old = _memory.Get<T>();
				_memory.Set<T>(t);

				if (old != t)
					_table.OnIntegerKeyChanged(old, t, this);
			}
			else
				_memory.Set<T>(t);
		}
	}

	template <typename T>
	inline void Set(const typename object_def::FieldType<T>::value_type * t, int32_t size) {
		if (_descriptor.Check<T>())
			_memory.Set<T>(t, size);
	}

	template <typename T>
	inline void SetString(const char * t) {
		if (_descriptor.Check<T>()) {
			if (object_def::Is<object_def::key, T>::value) {
				const char * old = _memory.Get<T>();
				if (strcmp(old, t) != 0) {
					std::string oldValue = old;
					_memory.SetString<T>(t);

					_table.OnStringKeyChanged(oldValue, t, this);
				}
				else
					_memory.SetString<T>(t);
			}
			else
				_memory.SetString<T>(t);
		}
	}

	inline TableRow * & Prev() { return _prev; }
	inline TableRow * & Next() { return _next; }

private:
	Memory _memory;

	TableControl& _table;
	TableDescriptor& _descriptor;

	TableRow * _prev = nullptr;
	TableRow * _next = nullptr;
};

class TableInterator {
public:
	TableInterator(TableRow * row) {};
	~TableInterator() {};

	inline TableRow * operator*() const { return _row; }

	TableInterator& operator++() {
		if (_row)
			_row = _row->Next();
		return *this;
	}

	TableInterator operator++(int) {
		TableInterator ret(_row);

		if (_row)
			_row = _row->Next();
		return ret;
	}

	inline bool operator==(const TableInterator& rhs) const {
		return _row == rhs._row;
	}

	inline bool operator!=(const TableInterator& rhs) const {
		return _row != rhs._row;
	}

private:
	TableRow * _row = nullptr;
};

class TableControl {
	friend class TableRow;
public:
	TableControl(TableDescriptor& desc) : _descriptor(desc) {}
	~TableControl() { clear(); }

	template <typename TAB, typename... Args>
	inline TableRow * FindRow(Args... args) {
		typedef typename object_def::TableKeyTraits<typename TAB::key_type>::type traits_type;
		return FindRowTrait(traits_type(), args...);
	}

	template <typename TAB, typename... Args>
	inline TableRow * AddRow(Args... args) {
		typedef typename object_def::TableKeyTraits<typename TAB::key_type>::type traits_type;
		return AddRowTrait<TAB>(traits_type(), args...);
	}

	template<typename TAB>
	inline void DelRow(TableRow * row) {
		typedef typename object_def::TableKeyTraits<typename TAB::key_type>::type traits_type;
		DelRowTrait<TAB>(traits_type(), row);
	}

	inline bool empty() const {
		return _head == nullptr;
	}

	inline TableInterator begin() {
		return TableInterator(_head);
	}

	inline TableInterator end() {
		return TableInterator(nullptr);
	}

	template <typename TAB>
	inline TableInterator erase(TableInterator& itr) {
		TableRow * row = *itr;
		if (row) {
			TableInterator ret(row->Next());

			DelRow<TAB>(row);
			return ret;
		}
		else
			return TableInterator(nullptr);
	}

	inline void clear() {
		TableRow * p = _head;
		while (p) {
			TableRow * c = p;
			p = p->Next();

			delete c;
		}

		_head = nullptr;
		_tail = nullptr;

		_intMap.clear();
		_stringMap.clear();
	}

private:
	inline TableRow * FindRowTrait(object_def::IntKey, int64_t key) {
		auto itr = _intMap.find(key);
		if (itr != _intMap.end())
			return itr->second;
		return nullptr;
	}

	inline TableRow * FindRowTrait(object_def::StringKey, const char * key) {
		auto itr = _stringMap.find(key);
		if (itr != _stringMap.end())
			return itr->second;
		return nullptr;
	}

	template <typename TAB>
	inline TableRow * AddRowTrait(object_def::IntKey, int64_t key) {
		if (_intMap.find(key) != _intMap.end())
			return nullptr;

		TableRow * row = new TableRow(*this, _descriptor);
		row->Set<typename TAB::key_type>(key);

		AddRow(row);
		_intMap[key] = row;
		return row;
	}

	template <typename TAB>
	inline TableRow * AddRowTrait(object_def::StringKey, const char * key) {
		if (_stringMap.find(key) != _stringMap.end())
			return nullptr;

		TableRow * row = new TableRow(*this, _descriptor);
		row->Set<typename TAB::key_type>(key);

		AddRow(row);
		_stringMap[key] = row;
		return row;
	}

	template <typename TAB>
	inline TableRow * AddRowTrait(object_def::NoKey) {
		TableRow * row = new TableRow(*this, _descriptor);
		AddRow(row);
		return row;
	}

	inline void AddRow(TableRow * row) {
		row->Prev() = _tail;
		if (_tail == nullptr)
			_head = _tail = row;
		else {
			_tail->Next() = row;
			_tail = row;
		}
	}

	template <typename TAB>
	inline void DelRowTrait(object_def::IntKey, TableRow * row) {
		typedef typename object_def::FieldType<typename TAB::key_type>::value_type value_type;
		value_type key = row->Get<typename TAB::key_type>();
		_intMap.erase(key);

		DelRowTrait<TAB>(object_def::NoKey(), row);
	}

	template <typename TAB>
	inline void DelRowTrait(object_def::StringKey, TableRow * row) {
		const char * key = row->Get<typename TAB::key_type>();
		_stringMap.erase(key);

		DelRowTrait<TAB>(object_def::NoKey(), row);
	}

	template <typename TAB>
	inline void DelRowTrait(object_def::NoKey, TableRow * row) {
		if (row->Next())
			row->Next()->Prev() = row->Prev();

		if (row->Prev())
			row->Prev()->Next() = row->Next();

		if (row == _head)
			_head = row->Next();

		if (row == _tail)
			_tail = _head->Prev();

		delete row;
	}

private:
	inline void OnIntegerKeyChanged(int64_t old, int64_t now, TableRow * row) {
		_intMap.erase(old);
		_intMap[now] = row;
	}

	inline void OnStringKeyChanged(const std::string& old, const std::string& now, TableRow * row) {
		_stringMap.erase(old);
		_stringMap[now] = row;
	}

private:
	TableDescriptor& _descriptor;

	TableRow * _head = nullptr;
	TableRow * _tail = nullptr;

	std::unordered_map<int64_t, TableRow*> _intMap;
	std::unordered_map<std::string, TableRow*> _stringMap;
};

#endif //defined __TABLECONTROL_H__
