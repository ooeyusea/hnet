#include "Cache.h"
#include "AccountCache.h"
#include "RoleCache.h"
#include "object_holder.h"
#include "mysqlmgr.h"
#include "XmlReader.h"
#include "dbdefine.h"

bool Cache::Start() {
	if (!OpenDB())
		return false;

	AccountCache::Instance().Start();
	RoleCache::Instance().Start();
	return true;
}

void Cache::Run() {
	int8_t res;
	_closeCh >> res;
}

void Cache::Release() {

}

void Cache::Terminate() {
	_closeCh.TryPush(1);
}

bool Cache::OpenDB() {
	olib::XmlReader conf;
	if (!conf.LoadXml("conf.xml")) {
		return false;
	}

	try {
		const char * dsn = conf.Root()["mysql"][0]["game_data"].GetAttributeString("dsn");
		auto * executor = MysqlMgr::Instance().Open(dsn);
		if (!executor) {
			return false;
		}

		Holder<MysqlExecutor, db_def::GAME> holder(executor);
	}
	catch (std::exception& e) {
		return false;
	}
	return true;
}