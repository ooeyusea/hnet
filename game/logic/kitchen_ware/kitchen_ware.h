#ifndef __KITCHEN_WARE_H__
#define __KITCHEN_WARE_H__
#include "hnet.h"
#include "eventdefine.h"
#include <map>
#include <set>
#include "conf.h"

class Object;
class KitchenWare : public Envir {
	struct Config {
		std::vector<int64_t> exps;
	};
public:
	KitchenWare();
	~KitchenWare() {}

	void Give(Object & obj, int32_t type, int32_t id, int32_t level, int64_t exp);
	void AddExp(Object & obj, int32_t type, int32_t id, int64_t add);
	void DelExp(Object & obj, int32_t type, int32_t id, int64_t sub);
	void Take(Object & obj, int32_t type, int32_t id);
	
	void Update(Object & obj, int32_t type, int32_t id, int32_t level);
	void Update(Object & obj, int32_t type, int32_t id, int32_t level, int64_t exp);

protected:
	bool ParseData(event_def::Data& data);
	void PackData(event_def::Data& data);

private:
	bool Initialize();
	bool LoadConfig();

	std::map<int32_t, std::map<int32_t, Config>> _wareConfigs;
};

extern KitchenWare g_kitcheWare;

#endif //__KITCHEN_WARE_H__
