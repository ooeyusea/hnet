#include "coroutine.h"

namespace hyper_net {
	Coroutine::Coroutine() {
		_ctx = nullptr;
		_status = CoroutineState::CS_RUNNABLE;
		_processer = nullptr;
		_inQueue = false;
	}

	Coroutine::Coroutine(const CoFuncType& f) {
		_fn = f;
		_ctx = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, Coroutine::CoroutineProc, this);
		_status = CoroutineState::CS_RUNNABLE;
		_processer = nullptr;
		_inQueue = false;
	}

	Coroutine::Coroutine(Coroutine&& rhs) {
		if (_ctx) {
			DeleteFiber(_ctx);
			_ctx = nullptr;
		}

		std::swap(_ctx, rhs._ctx);
		std::swap(_status, rhs._status);
		std::swap(_processer, rhs._processer);
		std::swap(_inQueue, rhs._inQueue);
	}

	Coroutine::~Coroutine() {
		if (_ctx)
			DeleteFiber(_ctx);
		_ctx = nullptr;
	}

	Coroutine& Coroutine::operator=(Coroutine&& rhs) {
		if (_ctx) {
			DeleteFiber(_ctx);
			_ctx = nullptr;
		}

		std::swap(_ctx, rhs._ctx);
		std::swap(_status, rhs._status);
		std::swap(_processer, rhs._processer);
		std::swap(_inQueue, rhs._inQueue);
		return *this;
	}

	void Coroutine::Run() {
		try {
			_fn();
			_fn = CoFuncType();
		}
		catch (...) {
			_fn = CoFuncType();

			std::exception_ptr eptr = std::current_exception();
		}
		_status = CoroutineState::CS_DONE;
		SwapOut();
	}

	void Coroutine::SwapIn() {
#ifndef USE_FCONTEXT
		SwitchToFiber(_ctx);
#else
		jump_fcontext(&GetTlsContext(), _ctx, vp_);
#endif
	}

	void Coroutine::SwapTo(Coroutine & co) {
#ifndef USE_FCONTEXT
		SwitchToFiber(co._ctx);
#else
#endif
	}

	void Coroutine::SwapOut() {
#ifndef USE_FCONTEXT
		SwitchToFiber(GetTlsContext());
#else
#endif
	}

	void Coroutine::CoroutineProc(void * p) {
		((Coroutine*)p)->Run();
	}
}