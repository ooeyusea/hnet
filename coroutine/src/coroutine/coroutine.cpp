#include "coroutine.h"

namespace hyper_net {
	Coroutine::Coroutine(const CoFuncType& f, int32_t stackSize) {
		_fn = f;
#ifndef USE_FCONTEXT
		_ctx = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, Coroutine::CoroutineProc, this);
#else
		_p = malloc(stackSize);
		//printf("this:0x%x _p:0x%x stackSize:%d\n", (int64_t)this, (int64_t)_p, stackSize);
		_ctx = make_fcontext((char*)_p + stackSize, stackSize, Coroutine::CoroutineProc);
#endif
		_status = CoroutineState::CS_RUNNABLE;
		_processer = nullptr;
		_inQueue = false;

		_temp = nullptr;
	}

	Coroutine::~Coroutine() {
		if (_ctx) {
#ifndef USE_FCONTEXT
			DeleteFiber(_ctx);
#else
			//printf("this:0x%x _p:0x%x\n", (int64_t)this, (int64_t)_p);
			free(_p);
#endif
		}
		_ctx = nullptr;
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
		jump_fcontext(&GetTlsContext(), _ctx, (intptr_t)this);
#endif
	}

	void Coroutine::SwapTo(Coroutine & co) {
#ifndef USE_FCONTEXT
		SwitchToFiber(co._ctx);
#else
		jump_fcontext(&_ctx, co._ctx, (intptr_t)&co);
#endif
	}

	void Coroutine::SwapOut() {
#ifndef USE_FCONTEXT
		SwitchToFiber(GetTlsContext());
#else
		jump_fcontext(&_ctx, GetTlsContext(), (intptr_t)nullptr);
#endif
	}

#ifndef USE_FCONTEXT
	void Coroutine::CoroutineProc(void * p) {
		((Coroutine*)p)->Run();
	}
#else 
	void Coroutine::CoroutineProc(intptr_t p) {
		((Coroutine*)p)->Run();
}
#endif
}