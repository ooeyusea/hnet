#include "async_queue.h"
#include "scheduler.h"
#include "hnet.h"

namespace hyper_net {
	void AsyncQueue::AsyncWorker::ThreadProc() {
		auto sweepFn = [this](Job * job) {
			_fn(job->data);
			Coroutine * co = job->co;
			delete job;

			Scheduler::Instance().AddCoroutine(co);
		};

		while (!_terminate) {
			_waitQueue.SweepOnce(sweepFn);

			std::this_thread::sleep_for(std::chrono::microseconds(1000));
		}

		if (_complete)
			_waitQueue.Sweep(sweepFn);
		else {
			_waitQueue.Sweep([](Job * job) {
				Coroutine * co = job->co;
				delete job;

				Scheduler::Instance().AddCoroutine(co);
			});
		}
	}

	AsyncQueue::AsyncQueue(int32_t threadCount, bool complete, const std::function<void(void * data)>& fn) : _fn(fn) {
		for (int32_t i = 0; i < threadCount; ++i) {
			AsyncWorker * worker = new AsyncWorker(complete, _fn);
			_workers.push_back(worker);
			worker->Start();
		}
	}

	AsyncQueue::~AsyncQueue() {
		for (auto * worker : _workers)
			delete worker;
		_workers.clear();
	}

	void AsyncQueue::Call(uint32_t threadId, void * data) {
		if (threadId < 0)
			return;

		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "is not in coroutine");

		uint32_t idx = threadId % (uint32_t)_workers.size();
		_workers[idx]->Add(co, data);

		co->SetStatus(CoroutineState::CS_BLOCK);
		hn_yield;
	}

	void AsyncQueue::Shutdown() {
		for (auto * worker : _workers)
			worker->Stop();

		for (auto * worker : _workers)
			worker->Join();

		delete this;
	}

	IAsyncQueue * CreateAsyncQueue(int32_t threadCount, bool complete, const std::function<void(void * data)>& f) {
		return new AsyncQueue(threadCount, complete, f);
	}
}

