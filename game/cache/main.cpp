#include "hnet.h"
#include "Cache.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"
#include "mysqlmgr.h"

void start(int32_t argc, char ** argv) {
	hn_info("Starting Cache ......");
	Cluster::Instance().SetType(node_def::CACHE);
	MysqlMgr::Instance();
	Cache cache;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("Cache id is {}", Cluster::Instance().GetId());

	if (!MysqlMgr::Instance().Start()) {
		return;
	}

	if (cache.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		hn_info("Cache {} Started", Cluster::Instance().GetId());

		cache.Run();
		cache.Release();

		hn_info("Cache {} Stoped", Cluster::Instance().GetId());
	}
}
