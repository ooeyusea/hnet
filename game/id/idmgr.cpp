#include "idmgr.h"
#include "initializor.h"
#include "XmlReader.h"
#include "servernode.h"
#include <thread>
#include "mysqlmgr.h"
#include "object_holder.h"
#include "dbdefine.h"

#define DEFAULT_POOL_SIZE (1024 * 1024)
#define DEFAULT_BATCH (8 * 1024)

IdMgr g_idMgr;

IdMgr::IdMgr() {
	InitializorMgr::Instance().AddStep("IdMgr#Init", [this]() {
		return Initialize();
	});
}

bool IdMgr::Initialize() {
	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

		if (conf.Root().IsExist("id_mgr") && conf.Root()["id_mgr"][0].IsExist("define")) {
			_poolSize = conf.Root()["id_mgr"][0]["define"][0].GetAttributeInt32("pool");
			_batchSize = conf.Root()["id_mgr"][0]["define"][0].GetAttributeInt32("batch");
		}
		else {
			_poolSize = DEFAULT_POOL_SIZE;
			_batchSize = DEFAULT_BATCH;
		}

		const char * dsn = conf.Root()["mysql"][0]["game_data"][0].GetAttributeString("dsn");
		auto * executor = MysqlMgr::Instance().Open(dsn);
		if (!executor) {
			return false;
		}

		Holder<MysqlExecutor, db_def::GAME> holder(executor);

		MysqlExecutor::ResultSet rs;
		int32_t ret = executor->Execute(0, "select `value` from `global` where `key` = 'id'", rs);
		if (ret != 0)
			return false;

		if (rs.Next()) {
			_nextId = rs.ToInt64(0);
			_saveId = _nextId + _poolSize;

			char sql[1024];
			SafeSprintf(sql, sizeof(sql), "update `global` set `value` = '%lld' where `key` = 'id'", _saveId);

			if (executor->Execute(0, sql) < 0)
				return false;
		}
		else {
			_nextId = 1;
			_saveId = _nextId + _poolSize;

			char sql[1024];
			SafeSprintf(sql, sizeof(sql), "insert into `global`(`key`, `value`) values('id', '%lld')", _saveId);

			if (executor->Execute(0, sql) < 0)
				return false;
		}
	}
	catch (std::exception& e) {
		hn_error("Load Player Manager Config : {}", e.what());
		return false;
	}

	Cluster::Instance().Get().Register(rpc_def::GET_ID).AddCallback<128>(*this, &IdMgr::GetId).Comit();
	return true;
}

rpc_def::GetIdAck IdMgr::GetId() {
	rpc_def::GetIdAck ack;
	ack.test = false;
	std::unique_lock<spin_mutex> lock(_mutex);
	if (_saveId - _nextId < _batchSize) {
		_saveId += _poolSize;

		char sql[1024];
		SafeSprintf(sql, sizeof(sql), "update `global` set `value` = '%lld' where `key` = 'id'", _saveId);
		if (Holder<MysqlExecutor, db_def::GAME>::Instance()->Execute(0, sql) > 0)
			return ack;
	}

	ack.test = true;
	ack.data.start = _nextId;
	ack.data.end = _nextId + _batchSize;

	return ack;
}
