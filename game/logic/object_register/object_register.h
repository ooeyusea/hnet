#ifndef __OBJECT_REGISTER_H__
#define __OBJECT_REGISTER_H__
#include "ObjectProp.h"

namespace object {
	DECL_ATTR(save_t);

	BEGIN_CLASS(Object, 0);
		DECL_PROP(object_type, 0, int32_t);
		PROP_COMIT_AFTER(object_type);

		TABLE_COMIT();
	END_CLASS();

	BEGIN_CLASS_INHERIT(Player, 0, Object);
		DECL_PROP(gate, 0, int16_t);
		DECL_PROP_AFTER(name, 0, char[32], gate);
		DECL_PROP(version, 0, int32_t);

		DECL_PROP_AFTER(recover_timer, 0, int64_t, version);
		DECL_PROP_AFTER(save_timer, 0, int64_t, recover_timer);
		PROP_COMIT_AFTER(save_timer);

		BEGIN_TABLE(KitchenWare);
		DECL_COLUMN(id, int64_t, key);
		DECL_COLUMN(level, int32_t, nokey);
		DECL_COLUMN(exp, int64_t, nokey);
		END_TABLE(id);

		TABLE_COMIT_AFTER(KitchenWare);
	END_CLASS();
}

#endif //__OBJECT_REGISTER_H__