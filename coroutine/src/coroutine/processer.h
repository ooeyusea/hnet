#ifndef __PROCESSER_H__
#define __PROCESSER_H__
#include "util.h"
#include "spin_mutex.h"
#include "coroutine.h"
#include "options.h"

namespace hyper_net {
	class Processer {
		Processer(Processer&) = delete;
		Processer(const Processer&) = delete;
		Processer(Processer&& rhs) = delete;

		Processer& operator=(Processer&) = delete;
		Processer& operator=(const Processer&) = delete;
		Processer& operator=(Processer&& rhs) = delete;

	public:
		Processer();
		~Processer();

		inline std::list<Coroutine *> Steal(int32_t n) {
			if (_runQueue.size() <= n || n == 0) {
				std::lock_guard<std::mutex> guard(_lock);
				return std::move(_runQueue);
			}
			else {
				std::list<Coroutine *> ret;
				std::lock_guard<std::mutex> guard(_lock);
				while (n--) {
					ret.push_back(*_runQueue.rbegin());
					_runQueue.pop_back();
				}
				return std::move(ret);
			}
		}

		inline void Add(Coroutine * co) {
			if (co->InQueue())
				return;

			while (!co->TestComplete())
				;

			co->SetQueue(true);
			co->SetProcesser(this);
			++_count;
			std::lock_guard<std::mutex> guard(_lock);
			_runQueue.push_front(co);
		}

		inline void Add(std::list<Coroutine *>&& coroutines) {
			for (auto * co : coroutines)
				co->SetProcesser(this);
			_count += (int32_t)coroutines.size();

			std::lock_guard<std::mutex> guard(_lock);
			std::copy(coroutines.begin(), coroutines.end(), std::back_inserter(_runQueue));
		}

		void Run();

		inline void CoYield() {
			OASSERT(_current, "where is running yield");

			_current->SwapOut();
		}

		inline static void StaticCoYield() {
			Processer * proc = GetProcesser();
			OASSERT(proc, "where is processer");

			proc->CoYield();
		}

		inline static Processer* & GetProcesser() {
			static thread_local Processer * proc = nullptr;
			return proc;
		}

		inline void Setup() {
			GetProcesser() = this;
		}

		inline void Mark() {
			if (_current && _markSwitch != _switch) {
				_markSwitch = _switch;
				_mark = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			}
		}

		inline bool IsBlocking() {
			if (!_current || !_markSwitch || _markSwitch != _switch)
				return false;

			int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			return now - _mark > Options::Instance().GetCycleTimeOut();
		}

		inline int32_t Count() const { return _count; }

		inline int32_t Speed() const { return _speed; }

		inline bool IsActive() const { return _active; }
		inline void SetActive(bool active) { _active = active; }

		inline void Stop() { _terminate = true; }

		inline bool IsWaiting() const { return _waiting; }

		inline void Wait() {
			_waiting = true;
		}

		inline void NotifyCondition() {
			_cond.notify_one();
		}

		inline Coroutine * Current() const { return _current; }

	private:
		inline Coroutine * Pop() {
			std::unique_lock<std::mutex> guard(_lock);
			if (_runQueue.empty()) {
				_waiting = true;
				_cond.wait(guard);
				_waiting = false;

				if (_runQueue.empty())
					return nullptr;
			}

			Coroutine * co = _runQueue.front();
			_runQueue.pop_front();
			return co;
		}

		inline void PushBack(Coroutine * co) {
			std::lock_guard<std::mutex> guard(_lock);
			_runQueue.push_back(co);
		}

	private:
		std::mutex _lock;
		std::condition_variable _cond;
		std::list<Coroutine *> _runQueue;
		Coroutine * _current;
		std::atomic<int32_t> _count;

		bool _active;
		bool _waiting;
		bool _terminate;

		int64_t _mark;
		uint64_t _switch;
		uint64_t _markSwitch;
		int32_t _speed;
	};
}

#endif // !__PROCESSER_H__
