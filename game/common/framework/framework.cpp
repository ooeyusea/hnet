#include "hnet.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"
#include "nodedefine.h"
#include "framework.h"
#include "initializor.h"

bool Framework::Start() {
	InitializorMgr::Instance().Start();
	return true;
}

void Framework::Run() {
	int8_t res;
	_closeCh >> res;
}

void Framework::Release() {

}

void Framework::Terminate() {
	_closeCh.TryPush(1);
}

void start(int32_t argc, char ** argv) {
	hn_info("Starting {} ......", argv[0]);
	Cluster::Instance().SetType(node_def::LOGIC);
	Framework framework;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("{} id is {}", argv[0], Cluster::Instance().GetId());

	if (framework.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		hn_info("{} {} Started", argv[0], Cluster::Instance().GetId());

		framework.Run();
		framework.Release();

		hn_info("{} {} Stoped", argv[0], Cluster::Instance().GetId());
	}
}
