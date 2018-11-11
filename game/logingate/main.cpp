#include "hnet.h"
#include "LoginGate.h"
#include "servernode.h"
#include "argument.h"

void start(int32_t argc, char ** argv) {
	Cluster::Instance();
	LoginGate gate;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		return;
	}

	if (gate.Start()) {
		if (!Cluster::Instance().Start())
			return;

		gate.Run();
		gate.Release();
	}
}
