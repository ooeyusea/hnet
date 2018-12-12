#include "ObjectMgr.h"
#include <thread>
#include "Object.h"
#include "id/id.h"

ObjectMgr g_objectMgr;

void ObjectMgr::Destroy() {
    auto itr = _objects.begin();
    while (itr != _objects.end()) {

		delete itr->second.object;
        itr++;
    }
	_objects.clear();
}

Object * ObjectMgr::Create(const char * file, const int32_t line, int32_t type, int64_t id) {
	if (id == 0) {
		id = IdGeter::Instance().Get();
		if (id == 0)
			return nullptr;
	}

    auto itr = _models.find(type);
    if (itr == _models.end()) {
        return nullptr;
    }

	std::unique_lock<spin_mutex> lock(_mutex);
	if (_objects.find(id) != _objects.end()) {
		return nullptr;
	}
	lock.unlock();

    Object * object = new Object(type, itr->second);
	object->SetID(id);
    if (nullptr == object) {
        return nullptr;
    }

	lock.lock();
	if (_objects.find(id) != _objects.end()) {
		lock.unlock();

		delete object;
		return nullptr;
	}

	_objects.insert(std::make_pair(id, ObjectCreateInfo{ object, file, line }));
    return object;
}

Object * ObjectMgr::FindObject(const int64_t id) {
	auto itr = _objects.find(id);
	if (itr == _objects.end())
		return nullptr;

	return itr->second.object;
}

void ObjectMgr::Recove(Object * object) {
    auto itr = _objects.find(object->GetID());
    if (itr == _objects.end()) {

        return;
    }

	delete object;
    _objects.erase(itr);
}
