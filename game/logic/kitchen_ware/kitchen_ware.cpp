#include "kitchen_ware.h"
#include "initializor.h"
#include "ObjectMgr.h"
#include "PlayerMgr.h"
#include "XmlReader.h"
#include "event_manager.h"
#include "eventdefine.h"
#include "object_register.h"

#define KW_ID(id) ((id) & 0xFFFFFFFF)
#define KW_TYPE(id) (((id) >> 32) & 0xFFFFFFFF)
#define KW_COMBINE(type, id) (((type) << 32) | id)

KitchenWare g_kitcheWare;

KitchenWare::KitchenWare() {
	InitializorMgr::Instance().AddStep("KitchenWare#LoadConfig", [this]() {
		return LoadConfig();
	});

	InitializorMgr::Instance().AddStep("KitchenWare#RegisterFunc", [this]() {
		return Initialize();
	}).MustInitAfter("PlayerMgr#Init").MustInitAfter("KitchenWare#LoadConfig");
}

bool KitchenWare::Initialize() {
	g_eventMgr.RegJudge<event_def::Data>(event_def::PACK_DATA, __FILE__, __LINE__, std::bind(&KitchenWare::ParseData, this, std::placeholders::_1));
	g_eventMgr.RegEvent<event_def::Data>(event_def::PACK_DATA, __FILE__, __LINE__, std::bind(&KitchenWare::PackData, this, std::placeholders::_1));
	return true;
}

bool KitchenWare::LoadConfig() {
	try {
		olib::XmlReader conf;
		if (!conf.LoadXml((_envir + "./kitchen_ware/conf.xml").c_str())) {
			return false;
		}

		for (const auto& ware : olib::ArrayIterator(conf.Root()["ware"])) {
			int32_t wareType = ware.GetAttributeInt32("type");
			int32_t wareId = ware.GetAttributeInt32("id");

			for (const auto& exp: olib::ArrayIterator(ware["level"])) {
				_wareConfigs[wareType][wareId].exps.push_back(exp.GetAttributeInt64("exp"));
			}
		}
	}
	catch (std::exception& e) {
		hn_error("Load Player Manager Config : {}", e.what());
		return false;
	}

	return true;
}

void KitchenWare::Give(Object & obj, int32_t type, int32_t id, int32_t level, int64_t exp) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(id);
	if (!row) {
		row = table->AddRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
		row->Set<object::Player::KitchenWare::level>(level);
		row->Set<object::Player::KitchenWare::exp>(exp);
	}
}

void KitchenWare::AddExp(Object & obj, int32_t type, int32_t id, int64_t add) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
	if (row) {
		auto itr = _wareConfigs[type].find(id);
		if (itr != _wareConfigs[type].end()) {
			int32_t oldLevel = row->Get<object::Player::KitchenWare::level>();
			int64_t oldExp = row->Get<object::Player::KitchenWare::exp>();

			int32_t level = oldLevel;
			int32_t exp = oldExp + add;
			while (level < itr->second.exps.size()) {
				if (exp >= itr->second.exps[level - 1]) {
					++level;
					exp -= itr->second.exps[level - 1];
				}
				else
					break;
			}

			row->Set<object::Player::KitchenWare::level>(level);
			row->Set<object::Player::KitchenWare::exp>(exp);
		}
	}
}

void KitchenWare::DelExp(Object & obj, int32_t type, int32_t id, int64_t sub) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
	if (row) {
		auto itr = _wareConfigs[type].find(id);
		if (itr != _wareConfigs[type].end()) {
			int32_t oldLevel = row->Get<object::Player::KitchenWare::level>();
			int64_t oldExp = row->Get<object::Player::KitchenWare::exp>();

			int32_t level = oldLevel;
			int32_t exp = oldExp - sub;
			while (exp < 0) {
				if (level <= 1) {
					exp = 0;
					break;
				}

				--level;
				exp += itr->second.exps[level - 1];
			}

			row->Set<object::Player::KitchenWare::level>(level);
			row->Set<object::Player::KitchenWare::exp>(exp);
		}
	}
}

void KitchenWare::Take(Object & obj, int32_t type, int32_t id) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
	if (row) {
		table->DelRow<object::Player::KitchenWare>(row);
	}
}

void KitchenWare::Update(Object & obj, int32_t type, int32_t id, int32_t level) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
	if (row) {
		row->Set<object::Player::KitchenWare::level>(level);
	}
}

void KitchenWare::Update(Object & obj, int32_t type, int32_t id, int32_t level, int64_t exp) {
	TableControl * table = obj.GetTable<object::Player::KitchenWare>();
	TableRow * row = table->FindRow<object::Player::KitchenWare>(KW_COMBINE(type, id));
	if (row) {
		row->Set<object::Player::KitchenWare::level>(level);
		row->Set<object::Player::KitchenWare::exp>(exp);
	}
}

bool KitchenWare::ParseData(event_def::Data& data) {
	TableControl * table = data.obj.GetTable<object::Player::KitchenWare>();
	for (auto& u : data.data.kitchenWares) {
		for (auto& v : u.second) {
			TableRow * row = table->AddRow<object::Player::KitchenWare>(KW_COMBINE(u.first, v.id));
			if (row) {
				row->Set<object::Player::KitchenWare::level>(v.level);
				row->Set<object::Player::KitchenWare::exp>(v.exp);
			}
		}
	}
	return true;
}

void KitchenWare::PackData(event_def::Data& data) {
	TableControl * table = data.obj.GetTable<object::Player::KitchenWare>();
	for (auto * row : *table) {
		int64_t id = row->Get<object::Player::KitchenWare::id>();
		data.data.kitchenWares[KW_TYPE(id)].push_back({ KW_ID(id), row->Get<object::Player::KitchenWare::level>(), row->Get<object::Player::KitchenWare::exp>() });
	}
}
