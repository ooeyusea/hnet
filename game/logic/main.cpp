#include "hnet.h"
#include "Logic.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"

void start(int32_t argc, char ** argv) {
	Cluster::Instance();
	Logic logic;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		return;
	}

	if (logic.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		logic.Run();
		logic.Release();
	}
}
