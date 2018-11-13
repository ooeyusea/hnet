#ifndef __TIMER_H__
#define __TIMER_H__
#include "util.h"
#include "coroutine.h"
#include "lock_free_list.h"

#define WEEL_COUNT 5

namespace hyper_net {
	struct WaitTimer {
		int64_t util;
		Coroutine * co;

		AtomicIntrusiveLinkedListHook<WaitTimer> next;
	};

	class TimerWeel {
	public:
		TimerWeel(int32_t size) { _degrees.resize(size); }
		~TimerWeel() {}

		void CheckHigher();
		void UpdateToLower();
		void Update();

		inline void Add(Coroutine * co, int64_t util, int32_t index) {
			OASSERT(index >= 0 && index < (int32_t)_degrees.size(), "time weel index error");
			_degrees[index].push_back({ util, co });
		}

		inline void Remove(Coroutine * co, int64_t util, int32_t index) {
			OASSERT(index >= 0 && index < (int32_t)_degrees.size(), "time weel index error");
			_degrees[index].remove_if([co](const auto& u) {
				return u.co == co;
			});
		}

		inline void SetHigher(TimerWeel * p) { _higher = p; }
		
	private:
		int32_t _hand = 0;
		TimerWeel * _higher = nullptr;

		std::vector<std::list<WaitTimer>> _degrees;
	};

	class TimerMgr {
		TimerMgr(TimerMgr&) = delete;
		TimerMgr(const TimerMgr&) = delete;
		TimerMgr(TimerMgr&& rhs) = delete;

		TimerMgr& operator=(TimerMgr&) = delete;
		TimerMgr& operator=(const TimerMgr&) = delete;
		TimerMgr& operator=(TimerMgr&& rhs) = delete;
	public:
		TimerMgr();
		~TimerMgr();

		void Process();
		void AddToWeel(Coroutine * co, int64_t util);
		void RemoveFromWeel(Coroutine * co, int64_t util);
		void PushWeel();

		static TimerMgr& Instance() {
			static TimerMgr g_instance;
			return g_instance;
		}

		inline void Add(Coroutine * co, int64_t util) {
			WaitTimer * unit = new WaitTimer{ util, co };
			_waitQueue.InsertHead(unit);
		}

		int32_t Sleep(int64_t millisecond);
		int32_t SleepUntil(int64_t util);

		inline void Remove(Coroutine * co) { _uses.erase(co); }

		inline void Stop(Coroutine * co) {
			WaitTimer * unit = new WaitTimer{ 0, co };
			_stopQueue.InsertHead(unit);
		}

	private:
		int64_t _steadyTick;
		int64_t _now;
		bool _terminate{ false };

		std::thread _t;

		std::vector<TimerWeel*> _tw;
		std::unordered_map<Coroutine*, int64_t> _uses;

		AtomicIntrusiveLinkedList<WaitTimer, &WaitTimer::next> _waitQueue;
		AtomicIntrusiveLinkedList<WaitTimer, &WaitTimer::next> _stopQueue;
	};
}

#endif // !__TIMER_H__
