#ifndef __OPTIONS_H__
#define __OPTIONS_H__
#include "util.h"

namespace hyper_net {
	class Options {
	public:
		void Setup(int32_t argc, char ** argv);

		static Options& Instance() {
			static Options g_instance;
			return g_instance;
		}

		inline int32_t GetMaxProcesser() const { return _maxP; }
		inline int32_t GetMinProcesser() const { return _minP; }

		inline int64_t GetCycleTimeOut() const { return _cycleTimeout; }
		inline int64_t GetDispatchThreadCycle() const { return _dispatchThreadCycle; }

		inline int32_t GetEpollEventSize() const { return _epollEventSize; }

	private:
		Options() {}
		~Options() {}

		int32_t _maxP;
		int32_t _minP;

		int64_t _cycleTimeout;
		int64_t _dispatchThreadCycle;

		int32_t _epollEventSize;
	};
}

#endif // !__OPTIONS_H__
