#include "scheduler.h"
#include "options.h"
#include "coroutine.h"
#include "processer.h"
#include "hnet.h"
#include "timer.h"
#include "net.h"
#include "logger.h"

extern void start(int32_t argc, char ** argv);

#define DEFAULT_SPEED 100
#define RATE 3

namespace hyper_net {
	Scheduler::Scheduler() {
		_terminate = false;
	}

	Scheduler::~Scheduler() {

	}

	int32_t Scheduler::Start(int32_t argc, char ** argv) {
		Options::Instance().Setup(argc, argv);
		if (!InitLogger())
			return -1;

		NetEngine::Instance();
		TimerMgr::Instance();

		_maxProcesser = Options::Instance().GetMaxProcesser();
		_minProcesser = Options::Instance().GetMinProcesser();
		_processers.reserve(_maxProcesser);

		Processer * p = new Processer;
		_processers.push_back(p);
		p->Setup();
		p->SetActive(true);

		for (int32_t i = 1; i < _minProcesser; ++i)
			AddNewProcesser();

		std::thread([this]() {
			DispatchThread();
		}).detach();

		hn_fork hn_stack(64 * 1024 * 1024) [argc, argv]{
			start(argc, argv);
		};

		p->Run();
		return 0;
	}

	void Scheduler::AddNewProcesser() {
		Processer * p = new Processer;

		std::thread([p](){
			p->Setup();
			p->Run();
		}).detach();

		_processers.push_back(p);
	}

	void Scheduler::DispatchThread() {
		while (!_terminate) {
			std::this_thread::sleep_for(std::chrono::microseconds(Options::Instance().GetDispatchThreadCycle()));
			if (_coroutineCount <= 0) {
				for (auto * p : _processers) {
					p->Stop();
					p->NotifyCondition();
				}
				_terminate = true;
				continue;
			}

			int32_t activeCount = 0;
			int32_t count = (int32_t)_processers.size();
			for (int32_t i = 0; i < count; ++i) {
				Processer * proc = _processers[i];
				if (proc->IsBlocking()) {
					if (proc->IsActive()) {
						proc->SetActive(false);
					}
				}

				if (proc->IsActive())
					++activeCount;
			}

			std::multimap<int32_t, int32_t> actives;
			std::map<int32_t, RunStat> runStats;
			std::map<int32_t, int32_t> blocking;

			int32_t totalLoadAvarge = 0;
			int32_t canActiveP = activeCount < _minProcesser ? _minProcesser - activeCount : 0;
			for (int32_t i = 0; i < count; ++i) {
				Processer * proc = _processers[i];
				if (!proc->IsActive()) {
					if (canActiveP > 0 && !proc->IsBlocking()) {
						--canActiveP;
						proc->SetActive(true);
					}
				}

				int32_t loadAvarge = proc->Count();
				totalLoadAvarge += loadAvarge;

				if (proc->IsActive()) {
					actives.insert(std::make_pair(loadAvarge, i));
					runStats[count] = { loadAvarge, proc->Speed() };
					proc->Mark();
				}
				else
					blocking[i] = loadAvarge;

				if (loadAvarge > 0 && proc->IsWaiting())
					proc->NotifyCondition();
			}

			if (totalLoadAvarge > 0 && actives.empty() && count < _maxProcesser) {
				AddNewProcesser();
				actives.insert(std::make_pair(0, count));
				runStats[count] = { 0, DEFAULT_SPEED };
				++count;
			}

			if (actives.empty())
				continue;

			std::list<Coroutine*> coroutines;
			for (auto itr = blocking.begin(); itr != blocking.end(); ++itr) {
				Processer * proc = _processers[itr->first];
				std::list<Coroutine*> tmp = proc->Steal(0);
				std::copy(tmp.begin(), tmp.end(), std::back_inserter(coroutines));
			}

			if (!coroutines.empty()) {
				auto range = actives.equal_range(actives.begin()->first);
				int32_t size = (int32_t)std::distance(range.first, range.second);
				int32_t avg = (int32_t)(coroutines.size() / size);
				avg = (avg == 0 ? 1 : avg);

				std::list<std::list<Coroutine*>> cuts = [&coroutines](int32_t size, int32_t avg) -> std::list<std::list<Coroutine*>> {
					std::list<std::list<Coroutine*>> ret;
					while (size--) {
						ret.push_back(std::list<Coroutine*>());

						int32_t c = avg;
						while (c-- && !coroutines.empty()) {
							ret.rbegin()->push_back(coroutines.front());
							coroutines.pop_front();
						}
					}

					if (!coroutines.empty())
						std::copy(coroutines.begin(), coroutines.end(), std::back_inserter(*ret.rbegin()));

					return std::move(ret);
				}(size, avg);

				for (auto itr = range.first; itr != range.second; ++itr) {
					if (!cuts.empty()) {
						auto& cut = cuts.front();
						Processer * proc = _processers[itr->second];

						runStats[count] = { (int32_t)cut.size(), proc->Speed() };
						proc->Add(std::move(cut));

						cuts.pop_front();
					}
				}
			}


			int32_t maxProc = -1;
			int32_t runMaxLen = 0;
			int32_t minProc = -1;
			int32_t runMinLen = 0;
			for (auto itr = runStats.begin(); itr != runStats.end(); ++itr) {
				if (maxProc == -1 || (itr->second.num > 1 && runMaxLen < itr->second.num * itr->second.speed)) {
					maxProc = itr->first;
					runMaxLen = itr->second.num * itr->second.speed;
				}

				if (minProc == -1 || runMinLen > itr->second.num * itr->second.speed) {
					minProc = itr->first;
					runMinLen = itr->second.num * itr->second.speed;
				}
			}

			if (maxProc != -1 && minProc != -1 && minProc != maxProc && runMaxLen >= runMinLen * RATE) {
				Processer * procMax = _processers[maxProc];
				std::list<Coroutine*> tmp = procMax->Steal(runStats[maxProc].num / 2);

				Processer * procMin = _processers[minProc];
				procMin->Add(std::move(tmp));
			}
		}
	}

	void Scheduler::AddCoroutine(const std::function<void()> f, int32_t stackSize) {
		AddCoroutine(new Coroutine(f, stackSize));
		_coroutineCount.fetch_add(1);
	}

	void Scheduler::AddCoroutine(Coroutine * co) {
		Processer * proc = co->GetProcesser();
		if (proc && proc->IsActive()) {
			proc->Add(co);
			return;
		}

		proc = Processer::GetProcesser();
		if (proc && proc->IsActive()) {
			proc->Add(co);
			return;
		}

		int32_t count = (int32_t)_processers.size();
		for (int32_t i = 0; i < count; ++i) {
			proc = _processers[i];
			if (proc->IsActive())
				break;
		}

		proc->Add(co);
	}

	Coroutine * Scheduler::CurrentCoroutine() {
		Processer * proc = Processer::GetProcesser();
		if (proc)
			return proc->Current();
		return nullptr;
	}
}
