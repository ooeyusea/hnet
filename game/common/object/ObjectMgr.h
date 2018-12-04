#ifndef __OBJECT_MGR_H__
#define __OBJECT_MGR_H__
#include "util.h"
#include <unordered_map>
#include "spin_mutex.h"
#include "ObjectProp.h"
#include "ObjectDescriptor.h"
#include "Object.h"

class ObjectMgr {
	struct ObjectCreateInfo {
		Object * object;
		std::string file;
		int32_t line;
	};

public:
	ObjectMgr() {}
	~ObjectMgr() {}

    void Destroy();

	template <typename T>
	inline Object * Create(const char * file, const int32_t line, int64_t id = 0) {
		return Create(file, line, T::type, id);
	}

	Object * Create(const char * file, const int32_t line, int32_t type, int64_t id = 0);
	Object * FindObject(const int64_t id);
    void Recove(Object * object);

	template <typename T>
	bool Register() {
		auto itr = _models.find(T::type);
		if (itr != _models.end()) {
			hn_info("type {} already register.", T::type);
			return false;
		}

		ObjectDescriptor desc;
		if (!desc.Setup<typename T>()) {
			hn_info("type {} register failed.", T::type);
			return false;
		}

		_models.insert(std::make_pair(T::type, desc));

		hn_info("type {} register success.", T::type);
		return true;
	}

private:
	std::unordered_map<std::string, std::string> _namePathMap;
	std::unordered_map<int32_t, ObjectDescriptor> _models;
	std::unordered_map<int64_t, ObjectCreateInfo> _objects;

	spin_mutex _mutex;
};

extern ObjectMgr g_objectMgr;
#endif //define __OBJECT_MGR_H__
