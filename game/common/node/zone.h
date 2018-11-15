#ifndef __ZONE_H__
#define __ZONE_H__
#include "hnet.h"
#include "nodedefine.h"
#include "spin_mutex.h"

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
		return Calc(service, zone, CalcUniqueId(0, key));
	}

	int16_t Calc(int8_t service, int16_t zone, int64_t key);

protected:
	inline int64_t Zone::CalcUniqueId(int64_t hash, const char * str) {
		return *str ? CalcUniqueId((hash * 131 + (*str)) % 4294967295, str + 1) : hash;
	}

private:
	Zone();
	~Zone() {}

	spin_mutex _mutex;
	int16_t _zoneTypeCount[MAX_ZONE][node_def::COUNT];
};

#endif //__ZONE_H__
