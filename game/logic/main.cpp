#include "hnet.h"
#include "Logic.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"
#include "nodedefine.h"

void start(int32_t argc, char ** argv) {
	hn_info("Starting Logic ......");
	Cluster::Instance().SetType(node_def::LOGIC);
	Logic logic;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("Logic id is {}", Cluster::Instance().GetId());

	if (logic.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		hn_info("Logic {} Started", Cluster::Instance().GetId());

		logic.Run();
		logic.Release();

		hn_info("Logic {} Stoped", Cluster::Instance().GetId());
	}
}
