#include "coroutine.h"
#include "options.h"
#ifndef WIN32
#include <sys/mman.h>
#include <cmath>
#endif

namespace hyper_net {
	thread_local Coroutine * g_now = nullptr;

#ifdef WIN32
	void * MallocStack(int32_t stackSize) {
		if (!Options::Instance().IsProtectStack()) {
			void * vp = malloc(stackSize);
			return (char*)vp + stackSize;
		}

		int32_t pageSize = Options::Instance().GetPageSize();
		int32_t pages = std::ceil((float)stackSize / (float)pageSize) + 1;
		int32_t realSize = pages * pageSize;

		void * vp = ::VirtualAlloc(0, realSize, MEM_COMMIT, PAGE_READWRITE);
		if (!vp) throw std::bad_alloc();

		DWORD old_options;
		const BOOL result = ::VirtualProtect(vp, pageSize, PAGE_READWRITE | PAGE_GUARD /*PAGE_NOACCESS*/, &old_options);

		return (char*)vp + realSize;
	}

	void FreeStack(void * p, int32_t stackSize) {
		if (!Options::Instance().IsProtectStack()) {
			free((char*)p - stackSize);
			return;
		}

		int32_t pageSize = Options::Instance().GetPageSize();
		int32_t pages = (int32_t)std::ceil((float)stackSize / (float)pageSize) + 1;
		int32_t realSize = pages * pageSize;

		void * vp = (char*)p - realSize;
		::VirtualFree(vp, 0, MEM_RELEASE);
	}
#else
	void * MallocStack(int32_t stackSize) {
		int32_t pageSize = Options::Instance().GetPageSize();
		int32_t pages = std::ceil((float)stackSize / (float)pageSize) + 1;
		int32_t realSize = pages * pageSize;

		void * vp = ::mmap(0, realSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (!vp) 
			throw std::bad_alloc();

		::mprotect(vp, pageSize, PROT_NONE);
		return (char*)vp + realSize;
}

	void FreeStack(void * p, int32_t stackSize) {
		int32_t pageSize = Options::Instance().GetPageSize();
		int32_t pages = std::ceil((float)stackSize / (float)pageSize) + 1;
		int32_t realSize = pages * pageSize;

		void * vp = (char*)p - realSize;
		::munmap(vp, realSize);
	}
#endif

	Coroutine::Coroutine(const CoFuncType& f, int32_t stackSize) {
		_flag.store(0);
		_fn = f;
#ifndef USE_FCONTEXT
		_ctx = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, Coroutine::CoroutineProc, this);
#else
		_p = MallocStack(stackSize);
		_stackSize = stackSize;
		//printf("this:0x%x _p:0x%x stackSize:%d\n", (int64_t)this, (int64_t)_p, stackSize);
		_ctx = make_fcontext((char*)_p, stackSize, Coroutine::CoroutineProc);
#endif
		_status = CoroutineState::CS_RUNNABLE;
		_processer = nullptr;
		_inQueue = false;
		_running = false;

		_temp = nullptr;
	}

	Coroutine::~Coroutine() {
		if (_ctx) {
#ifndef USE_FCONTEXT
			DeleteFiber(_ctx);
#else
			//printf("this:0x%x _p:0x%x\n", (int64_t)this, (int64_t)_p);
			FreeStack(_p, _stackSize);
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
		OASSERT(!_running, "already in running");

		_running = true;

		g_now = this;
#ifndef USE_FCONTEXT
		SwitchToFiber(_ctx);
#else
		jump_fcontext(&GetTlsContext(), _ctx, (intptr_t)this);
#endif
	}

	void Coroutine::SwapOut() {
		OASSERT(_running, "not running");

		_running = false;

		g_now = this;
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