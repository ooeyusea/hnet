#include "Cache.h"
#include "AccountCache.h"
#include "RoleCache.h"
#include "object_holder.h"
#include "mysqlmgr.h"
#include "XmlReader.h"
#include "dbdefine.h"
#include "argument.h"

#define ACCOUNT_RECOVER_TIMEOUT (360 * 1000)
#define SAVE_INTERVAL (360 * 1000)
#define ROLE_RECOVER_TIMEOUT (24 * 3600 * 1000)

bool Cache::Start() {
	if (!OpenDB())
		return false;

	int32_t accountTimeout, saveInterval, roleTimeout;

	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

		if (conf.Root().IsExist("cache") && conf.Root()["cache"][0].IsExist("define")) {
			accountTimeout = conf.Root()["cache"][0]["define"][0].GetAttributeInt32("account_timeout");
			saveInterval = conf.Root()["cache"][0]["define"][0].GetAttributeInt32("save");
			roleTimeout = conf.Root()["cache"][0]["define"][0].GetAttributeInt32("role_timeout");
		}
		else {
			accountTimeout = ACCOUNT_RECOVER_TIMEOUT;
			saveInterval = SAVE_INTERVAL;
			roleTimeout = ROLE_RECOVER_TIMEOUT;
		}
	}
	catch (std::exception& e) {
		hn_error("Load Cache Config : {}", e.what());
		return false;
	}

	hn_info("cache define account_timeout {} save_interval {} role_timeout {}", accountTimeout, saveInterval, roleTimeout);

	AccountCache::Instance().Start(accountTimeout);
	RoleCache::Instance().Start(saveInterval, roleTimeout);
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
	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

		const char * dsn = conf.Root()["mysql"][0]["game_data"][0].GetAttributeString("dsn");
		auto * executor = MysqlMgr::Instance().Open(dsn);
		if (!executor) {
			return false;
		}

		Holder<MysqlExecutor, db_def::GAME> holder(executor);
	}
	catch (std::exception& e) {
		hn_error("Load Cache Config : {}", e.what());
		return false;
	}
	return true;
}