#include "hnet.h"
#include "LoginGate.h"
#include "servernode.h"
#include "argument.h"
#include "nodedefine.h"

void start(int32_t argc, char ** argv) {
	hn_info("Starting LoginGate ......");
	Cluster::Instance().SetType(node_def::LOGINGATE);
	LoginGate gate;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("LoginGate id is {}", Cluster::Instance().GetId());

	if (gate.Start()) {
		if (!Cluster::Instance().Start())
			return;

		hn_info("LoginGate {} Started", Cluster::Instance().GetId());

		gate.Run();
		gate.Release();

		hn_info("LoginGate {} Stoped", Cluster::Instance().GetId());
	}
}
