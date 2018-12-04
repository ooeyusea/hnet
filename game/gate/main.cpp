#include "hnet.h"
#include "Gate.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"

void start(int32_t argc, char ** argv) {
	hn_info("Starting Gate ......");
	Cluster::Instance().SetType(node_def::GATE);
	Gate gate;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("Gate id is {}", Cluster::Instance().GetId());

	if (!Zone::Instance().Start())
		return;

	if (!Cluster::Instance().Start())
		return;

	if (gate.Start()) {
		hn_info("Gate {} Started", Cluster::Instance().GetId());

		gate.Run();
		gate.Release();

		hn_info("Gate {} Stoped", Cluster::Instance().GetId());
	}
}
