#ifndef __OBJECTDESCRIPTOR_H__
#define __OBJECTDESCRIPTOR_H__
#include "util.h"
#include "ObjectProp.h"
#include <vector>
#include <unordered_map>

namespace object_def {
	template <typename T>
	struct PropLoader;

	template <int32_t s, int32_t k, typename T, typename Before, typename... Args>
	struct PropLoader<Field<s, k, T, Before, Args...>> {
		inline static void invoke(std::vector<Prop>& props) {
			PropLoader<Before>::invoke(props);

			props.push_back({
				Field<s, k, T, Before, Args...>::key,
				Field<s, k, T, Before, Args...>::index,
				Field<s, k, T, Before, Args...>::offset,
				Field<s, k, T, Before, Args...>::space,
				Field<s, k, T, Before, Args...>::size,
				Field<s, k, T, Before, Args...>::setting,
			});
		}
	};

	template <typename T, int32_t s>
	struct PropLoader<FiledStart<T, s>> {
		inline static void invoke(std::vector<Prop>& props) {
		}
	};

	template <typename T, int32_t s, typename Before>
	struct PropLoader<FiledStart<T, s, Before>> {
		inline static void invoke(std::vector<Prop>& props) {
			PropLoader<Before>::invoke(props);
		}
	};
}

class TableDescriptor {
public:
	TableDescriptor() {}
	~TableDescriptor() {}

	template <typename T>
	inline bool Setup() {
		typedef typename T::__end hook_type;

		_type = T::type;
		object_def::PropLoader<hook_type>::invoke(_columns);
		_space = (_columns.empty() ? 0 : _columns.rbegin()->space);

		return true;
	}

	template <typename T>
	inline bool Check() {
		if (T::seq >= _columns.size())
			return false;

		return _columns[T::seq - 1].key == T::key;
	}

	inline int32_t GetType() const { return _type; }
	inline int32_t GetSpace() const { return _space; }

private:
	int32_t _type;
	int32_t _space = 0;

	std::vector<object_def::Prop> _columns;
};

namespace object_def {
	template <typename T>
	struct TableLoader {
		inline static bool invoke(std::vector<TableDescriptor>& tables) {
			if (!TableLoader<typename T::before_type>::invoke(tables))
				return false;

			TableDescriptor desc;
			if (!desc.Setup<T>())
				return false;

			return true;
		}
	};

	template <typename T>
	struct TableLoader<TableStart<T>> {
		inline static bool invoke(std::vector<TableDescriptor>& tables) {
			return true;
		}
	};

	template <typename T, typename Before>
	struct TableLoader<TableStart<T, Before>> {
		inline static bool invoke(std::vector<TableDescriptor>& tables) {
			return TableLoader<Before>::invoke(tables);
		}
	};
}

class ObjectDescriptor {
public:
	ObjectDescriptor() {}
	~ObjectDescriptor() {}

	template <typename T>
	inline bool Setup() {
		typedef typename T::__end hook_type;
		typedef typename T::__tableEnd hook_table_type;

		_type = T::type;
		object_def::PropLoader<hook_type>::invoke(_props);
		object_def::TableLoader<hook_table_type>::invoke(_tables);
		_space = (_props.empty() ? 0 : _props.rbegin()->space);

		for (auto& prop : _props) {
			auto itr = _propMap.find(prop.key);
			if (itr != _propMap.end())
				return false;

			_propMap.insert(std::make_pair(prop.key, &prop));
		}
		return true;
	}

	template <typename T>
	inline bool Check() {
		if (T::seq >= _props.size())
			return false;

		return _props[T::seq - 1].key == T::key;
	}

	template<typename T>
	inline bool CheckTable() {
		if (T::seq >= _tables.size())
			return false;

		return _tables[T::seq - 1].GetType() == T::type;
	}

	template <typename T>
	inline object_def::Prop& Get() {
		return _props[T::seq - 1];
	}

	inline int32_t GetType() const { return _type; }
	inline int32_t GetSpace() const { return _space; }
	std::vector<TableDescriptor> GetTableDesc() { return _tables; }

private:
	int32_t _type;
	int32_t _space = 0;

	std::vector<object_def::Prop> _props;
	std::unordered_map<int32_t, object_def::Prop *> _propMap;

	std::vector<TableDescriptor> _tables;
};

#endif //defined __OBJECTDESCRIPTOR_H__
