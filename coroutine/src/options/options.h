#ifndef __OPTIONS_H__
#define __OPTIONS_H__
#include "util.h"
#include <string>

namespace hyper_net {
	class Options {
	public:
		void Setup(int32_t argc, char ** argv);

		static Options& Instance() {
			static Options g_instance;
			return g_instance;
		}

		const char * GetExeName() const { return _exeName.c_str(); }

		inline int32_t GetMaxProcesser() const { return _maxP; }
		inline int32_t GetMinProcesser() const { return _minP; }

		inline int64_t GetCycleTimeOut() const { return _cycleTimeout; }
		inline int64_t GetDispatchThreadCycle() const { return _dispatchThreadCycle; }

		inline int32_t GetEpollEventSize() const { return _epollEventSize; }

		inline int32_t GetPageSize() const { return _pageSize; }
		inline bool IsProtectStack() const { return _protectStack; }
		inline int32_t GetDefaultStackSize() const { return _defaultStackSize; }
		inline int32_t GetMinStackSize() const { return _minStackSize; }

		inline int32_t GetLoggerQueueSize() const { return _loggerQueueSize; }
		inline int32_t GetLoggerFileSize() const { return _loggerFileSize; }
		inline int32_t GetLogThread() const { return _loggerThread; }
		inline bool IsLogConsole() const { return _loggerConsole; }
		inline int32_t GetLoggerLevel() const { return _loggerLevel; }

	private:
		Options() {}
		~Options() {}

		std::string _exeName;

		int32_t _maxP;
		int32_t _minP;

		int64_t _cycleTimeout;
		int64_t _dispatchThreadCycle;

		int32_t _epollEventSize;

		int32_t _pageSize;
		bool _protectStack;
		int32_t _defaultStackSize;
		int32_t _minStackSize;

		int32_t _loggerQueueSize;
		int32_t _loggerFileSize;
		int32_t _loggerThread;
		bool _loggerConsole;
		int32_t _loggerLevel;
	};
}

#endif // !__OPTIONS_H__
