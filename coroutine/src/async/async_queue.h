#ifndef __ASYNC_QUEUE_H__
#define __ASYNC_QUEUE_H__
#include "util.h"
#include "coroutine.h"
#include "lock_free_list.h"
#include "hnet.h"

namespace hyper_net {
	class AsyncQueue : public IAsyncQueue {
		struct Job {
			Coroutine * co;
			void * data;

			olib::AtomicIntrusiveLinkedListHook<Job> next;
		};

		class AsyncWorker {
		public:
			AsyncWorker(bool complete, const std::function<void(void * data)>& f) : _complete(complete), _fn(f) {
				_t = std::thread(&AsyncWorker::ThreadProc, this);
			}

			~AsyncWorker() {}

			inline void Add(Coroutine * co, void * data) { _waitQueue.InsertHead(new Job{ co, data }); }
			void ThreadProc();

			inline void Stop() { _terminate = true; }
			inline void Join() { _t.join(); }

		private:
			const std::function<void(void * data)>& _fn;

			olib::AtomicIntrusiveLinkedList<Job, &Job::next> _waitQueue;

			std::thread _t;
			bool _terminate = false;
			bool _complete;
		};
	public:
		AsyncQueue(int32_t threadCount, bool complete, const std::function<void(void * data)>& fn);
		~AsyncQueue();

		virtual void Call(uint32_t threadId, void * data);
		virtual void Shutdown();

	private:
		std::vector<AsyncWorker *> _workers;
		std::function<void(void * data)> _fn;
	};
}

#endif // !__ASYNC_QUEUE_H__
