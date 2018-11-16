#include "processer.h"
#include "scheduler.h"

namespace hyper_net {
	Processer::Processer() {
		_current = nullptr;
		_terminate = false;
		_active = false;
		_waiting = false;

		_count = 0;
		_markSwitch = 0;
		_mark = 0;
		_switch = 0;

		_speed = 0;
	}

	Processer::~Processer() {
		OASSERT(_runQueue.empty(), "process must has no coroutine when recover");
	}

	void Processer::Run() {
		Coroutine::SetupTlsContext();
		while (!_terminate) {
			_current = Pop();
			if (_current) {
				if (_switch + 1 == 0)
					_switch = 1;
				else
					++_switch;

				int64_t mark = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

				_current->Acquire();
				_current->SetQueue(false);
				_current->SetStatus(CoroutineState::CS_RUNNABLE);
				_current->SwapIn();
				_current->Release();

				int64_t markEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				_speed = (int32_t)((markEnd - mark + _speed) / 2);

				switch (_current->GetStatus()) {
				case CoroutineState::CS_RUNNABLE: PushBack(_current); break;
				case CoroutineState::CS_BLOCK:
					--_count;
					_current = nullptr;
					break;
				case CoroutineState::CS_DONE:
					--_count;
					delete _current;
					_current = nullptr;
					Scheduler::Instance().DecCoroutineCount();
					break;
				}
			}
			else {
				std::this_thread::sleep_for(std::chrono::microseconds(1000));
			}
		}
	}

}