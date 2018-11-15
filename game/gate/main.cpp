#include "hnet.h"
#include "Gate.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"

void start(int32_t argc, char ** argv) {
	Cluster::Instance();
	Gate gate;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		return;
	}

	if (!Zone::Instance().Start())
		return;

	if (!Cluster::Instance().Start())
		return;

	if (gate.Start()) {
		gate.Run();
		gate.Release();
	}
}
