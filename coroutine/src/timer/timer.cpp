#include "timer.h"
#include "scheduler.h"
#include "hnet.h"

enum {
	TQ_TVN_BITS = 6,
	TQ_TVR_BITS = 8,
	TQ_TVN_SIZE = 1 << TQ_TVN_BITS,
	TQ_TVR_SIZE = 1 << TQ_TVR_BITS,
	TQ_TVN_MASK = TQ_TVN_SIZE - 1,
	TQ_TVR_MASK = TQ_TVR_SIZE - 1,
};

namespace hyper_net {
	void TimerWeel::CheckHigher() {
		if (_hand >= (int32_t)_degrees.size())
			_hand = 0;

		if (_hand == 0) {
			if (_higher)
				_higher->UpdateToLower();
		}
	}

	void TimerWeel::UpdateToLower() {
		if (_hand >= (int32_t)_degrees.size())
			_hand = 0;

		if (_hand == 0) {
			if (_higher)
				_higher->UpdateToLower();
		}

		std::list<WaitTimer> tmp;
		tmp.swap(_degrees[_hand]);

		for (auto& u : tmp)
			TimerMgr::Instance().AddToWeel(u.co, u.util);

		++_hand;
	}
	
	void TimerWeel::Update() {
		if (_hand >= (int32_t)_degrees.size())
			_hand = 0;

		std::list<WaitTimer> tmp;
		tmp.swap(_degrees[_hand]);

		//Maybe use a thread to add
		for (auto& u : tmp) {
			TimerMgr::Instance().Remove(u.co);
			Scheduler::Instance().AddCoroutine(u.co);
		}

		++_hand;
	}

	TimerMgr::TimerMgr() {
		_steadyTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		_tw.push_back(new TimerWeel(TQ_TVR_SIZE));
		_tw.push_back(new TimerWeel(TQ_TVN_SIZE));
		_tw.push_back(new TimerWeel(TQ_TVN_SIZE));
		_tw.push_back(new TimerWeel(TQ_TVN_SIZE));
		_tw.push_back(new TimerWeel(TQ_TVN_SIZE));

		_tw[0]->SetHigher(_tw[1]);
		_tw[1]->SetHigher(_tw[2]);
		_tw[2]->SetHigher(_tw[3]);
		_tw[3]->SetHigher(_tw[4]);

		_t = std::thread([]() {
			TimerMgr::Instance().Process();
		});
	}

	TimerMgr::~TimerMgr() {
		_terminate = true;
		_t.join();
		for (auto * w : _tw)
			delete w;
		_tw.clear();
	}

	void TimerMgr::Process() {
		while (!_terminate) {
			_waitQueue.SweepOnce([this](WaitTimer * t) {
				_uses[t->co] = t->util;
				AddToWeel(t->co, t->util);
				delete t;
			});

			_stopQueue.SweepOnce([this](WaitTimer * t) {
				auto itr = _uses.find(t->co);
				if (itr != _uses.end()) {
					RemoveFromWeel(t->co, itr->second);
					Scheduler::Instance().AddCoroutine(t->co);
				}
				delete t;
			});

			int64_t tick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - _steadyTick;
			while (_now < tick && !_terminate)
				PushWeel();

			std::this_thread::sleep_for(std::chrono::microseconds(1000));
		}
	}

	void TimerMgr::AddToWeel(Coroutine * co, int64_t util) {
		int64_t expire = util > _steadyTick ? util - _steadyTick : 0;
		if (expire < _now)
			expire = _now;

		int64_t diff = expire - _now;

		if (diff < TQ_TVR_SIZE)
			_tw[0]->Add(co, util, expire & TQ_TVR_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + TQ_TVN_BITS)))
			_tw[1]->Add(co, util, (expire >> TQ_TVR_BITS) & TQ_TVN_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + 2 * TQ_TVN_BITS)))
			_tw[2]->Add(co, util, (expire >> (TQ_TVR_BITS + TQ_TVN_BITS)) & TQ_TVN_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + 3 * TQ_TVN_BITS)))
			_tw[3]->Add(co, util, (expire >> (TQ_TVR_BITS + 2 * TQ_TVN_BITS)) & TQ_TVN_MASK);
		else 
			_tw[4]->Add(co, util, (expire >> (TQ_TVR_BITS + 3 * TQ_TVN_BITS)) & TQ_TVN_MASK);
	}

	void TimerMgr::RemoveFromWeel(Coroutine * co, int64_t util) {
		int64_t expire = util > _steadyTick ? util - _steadyTick : 0;
		if (expire < _now)
			expire = _now;

		int64_t diff = expire - _now;

		if (diff < TQ_TVR_SIZE)
			_tw[0]->Remove(co, util, expire & TQ_TVR_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + TQ_TVN_BITS)))
			_tw[1]->Remove(co, util, (expire >> TQ_TVR_BITS) & TQ_TVN_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + 2 * TQ_TVN_BITS)))
			_tw[2]->Remove(co, util, (expire >> (TQ_TVR_BITS + TQ_TVN_BITS)) & TQ_TVN_MASK);
		else if (diff < (1 << (TQ_TVR_BITS + 3 * TQ_TVN_BITS)))
			_tw[3]->Remove(co, util, (expire >> (TQ_TVR_BITS + 2 * TQ_TVN_BITS)) & TQ_TVN_MASK);
		else
			_tw[4]->Remove(co, util, (expire >> (TQ_TVR_BITS + 3 * TQ_TVN_BITS)) & TQ_TVN_MASK);
	}

	void TimerMgr::PushWeel() {
		_tw[0]->CheckHigher();
		++_now;
		_tw[0]->Update();
	}

	int32_t TimerMgr::Sleep(int64_t millisecond) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must run in coroutine");

		int64_t util = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + millisecond;
		Add(co, util);

		co->SetStatus(CoroutineState::CS_BLOCK);

		hn_yield;

		return 0; //check failed
	}

	int32_t TimerMgr::SleepUntil(int64_t util) {
		Coroutine * co = Scheduler::Instance().CurrentCoroutine();
		OASSERT(co, "must run in coroutine");

		Add(co, util);

		co->SetStatus(CoroutineState::CS_BLOCK);

		hn_yield;

		return 0; //check failed
	}
}
