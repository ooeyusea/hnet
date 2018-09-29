#include "options.h"

namespace hyper_net {
	void Options::Setup(int32_t argc, char ** argv) {
		_maxP = std::thread::hardware_concurrency() * 2;
		_minP = std::thread::hardware_concurrency();

		_cycleTimeout = 10 * 1000;
		_dispatchThreadCycle = 1000;

		_epollEventSize = 512;
	}
}
