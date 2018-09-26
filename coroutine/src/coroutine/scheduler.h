#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
#include "util.h"

namespace hyper_net {
	struct RunStat{
		int32_t num;
		int32_t speed;
	};

	class Coroutine;
	class Processer;
	class Scheduler {
		Scheduler(Scheduler&) = delete;
		Scheduler(const Scheduler&) = delete;
		Scheduler(Scheduler&& rhs) = delete;

		Scheduler& operator=(Scheduler&) = delete;
		Scheduler& operator=(const Scheduler&) = delete;
		Scheduler& operator=(Scheduler&& rhs) = delete;
	public:
		Scheduler();
		~Scheduler();

		int32_t Start(int32_t argc, char ** argv);

		static Scheduler& Instance() {
			static Scheduler g_instance;
			return g_instance;
		}

		void AddNewProcesser();
		void DispatchThread();
		
		void AddCoroutine(const std::function<void()> f, int32_t stackSize);
		void AddCoroutine(Coroutine * co);

		inline void DecCoroutineCount() { _coroutineCount.fetch_sub(1); }

		Coroutine * CurrentCoroutine();

	private:
		std::vector<Processer *> _processers;

		int32_t _minProcesser;
		int32_t _maxProcesser;

		std::atomic<int32_t> _coroutineCount;

		bool _terminate;
	};
}

#endif // !__SCHEDULER_H__
