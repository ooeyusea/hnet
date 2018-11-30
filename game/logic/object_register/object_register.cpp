#include "object_register.h"
#include "initializor.h"
#include "ObjectMgr.h"

class ObjectRegister {
public:
	ObjectRegister() {
		InitializorMgr::Instance().AddStep("Object#Register#1", [this]() {
			return Init();
		});
	}

	~ObjectRegister() {}

	bool Init();
};

ObjectRegister g_objectRegister;

bool ObjectRegister::Init() {
	if (!g_objectMgr.Register<object::Player>())
		return false;

	return true;
}
