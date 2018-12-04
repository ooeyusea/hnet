#include "zone.h"
#include "servernode.h"

Zone::Zone() {
	memset(_zoneTypeCount, 0, sizeof(_zoneTypeCount));
}

bool Zone::Start() {
	Cluster::Instance().RegisterServiceOpen([this](int8_t service, int16_t id) {
		int16_t zoneId = ID_FROM_ID(id);
		int16_t zone = ZONE_FROM_ID(id);

		std::lock_guard<spin_mutex> guard(_mutex);
		if (_zoneTypeCount[zone][service] < zoneId + 1)
			_zoneTypeCount[zone][service] = zoneId + 1;
	});

	hn_info("zone module register complte.");
	return true;
}

int16_t Zone::Calc(int8_t service, int16_t zone, int64_t key) {
	if (_zoneTypeCount[zone][service] > 0)
		return (int16_t)((uint64_t)key % (uint64_t)_zoneTypeCount[zone][service]);

	return 0;
}
