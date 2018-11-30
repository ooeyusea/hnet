#include "Object.h"

Object::Object(int32_t type, ObjectDescriptor& desc)
	: _type(type)
	, _descriptor(desc)
	, _memory(desc.GetSpace()) {

	for (auto& desc : _descriptor.GetTableDesc())
		_tables.push_back(TableControl(desc));
}

Object::~Object() {
}
