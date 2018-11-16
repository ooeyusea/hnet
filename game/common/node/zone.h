#ifndef __ZONE_H__
#define __ZONE_H__
#include "hnet.h"
#include "nodedefine.h"
#include "spin_mutex.h"
#include "util.h"

class Zone {
public:
	static Zone& Instance() {
		static Zone g_instance;
		return g_instance;
	}

	bool Start();

	inline int16_t Calc(int8_t service, int16_t zone, const std::string& key) {
		return Calc(service, zone, key.c_str());
	}

	inline int16_t Calc(int8_t service, int16_t zone, const char * key) {
		return Calc(service, zone, util::CalcUniqueId(key));
	}

	int16_t Calc(int8_t service, int16_t zone, int64_t key);

private:
	Zone();
	~Zone() {}

	spin_mutex _mutex;
	int16_t _zoneTypeCount[MAX_ZONE][node_def::COUNT];
};

#endif //__ZONE_H__
