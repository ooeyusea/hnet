#include "hnet.h"
#include "Gate.h"

void start(int32_t argc, char ** argv) {
	Gate gate;
	if (gate.Start())
		gate.Run();
	gate.Release();
}
