#include "options.h"
#define MIN_STACK_SIZE (128 * 1024)
#define DEFAULT_STACK_SIZE (1 * 1024 * 1024)

namespace hyper_net {
	void Options::Setup(int32_t argc, char ** argv) {
		
#ifdef WIN32
		const char * start = strrchr(argv[0], '\\');
#else
		const char * start = strrchr(argv[0], '/');
#endif
		start = start ? start + 1 : argv[0];
		const char * p = strchr(start, '.');
		if (p)
			_exeName.assign(start, p - start);
		else
			_exeName = start;

		_maxP = std::thread::hardware_concurrency() * 2;
		_minP = std::thread::hardware_concurrency();

		_cycleTimeout = 10 * 1000;
		_dispatchThreadCycle = 1000;

		_epollEventSize = 512;
		_maxSocketCount = 65536;

#ifdef WIN32
		SYSTEM_INFO sysInfo;
		::GetSystemInfo(&sysInfo);
		_pageSize = (int32_t)sysInfo.dwPageSize;
#else
		_pageSize = ::sysconf(_SC_PAGESIZE);
#endif

		_protectStack = true;
		_defaultStackSize = DEFAULT_STACK_SIZE;
		_minStackSize = MIN_STACK_SIZE;


		_loggerQueueSize = 4096;
		_loggerFileSize = 50 * 1024 * 1024;
		_loggerThread = 1;
		_loggerConsole = true;
		_loggerLevel = 2;

		_debug = true;
	}
}
