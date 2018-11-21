#include "hnet.h"
#include "Cache.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"
#include "mysqlmgr.h"

void start(int32_t argc, char ** argv) {
	Cluster::Instance();
	Cache cache;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		return;
	}

	if (!MysqlMgr::Instance().Start()) {
		return;
	}

	if (cache.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		cache.Run();
		cache.Release();
	}
}
