#ifndef __COROUTINE_H__
#define __COROUTINE_H__
#include "util.h"
#ifdef USE_FCONTEXT
#include "fcontext.hpp"
#endif
#include <atomic>

namespace hyper_net {
	enum class CoroutineState {
		CS_RUNNABLE,
		CS_BLOCK,
		CS_DONE,
	};

#ifndef USE_FCONTEXT
	typedef void * fcontext_t;
#endif

	typedef std::function<void()> CoFuncType;

	class Processer;
	class Coroutine {
		Coroutine(Coroutine&) = delete;
		Coroutine(const Coroutine&) = delete;

		Coroutine& operator=(Coroutine&) = delete;
		Coroutine& operator=(const Coroutine&) = delete;

	public:
		Coroutine() = delete;
		Coroutine(const CoFuncType& f, int32_t stackSize);
		Coroutine(Coroutine&& rhs) = delete;
		~Coroutine();

		Coroutine& operator=(Coroutine&& rhs) = delete;

		void Run();
		void SwapIn();
		void SwapOut();

#ifndef USE_FCONTEXT
		static void CoroutineProc(void * p);
#else
		static void CoroutineProc(intptr_t p);
#endif

		inline static fcontext_t& GetTlsContext() {
			static thread_local fcontext_t tls_context;
			return tls_context;
		}

		inline static void SetupTlsContext() {
#ifndef USE_FCONTEXT
			GetTlsContext() = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
#endif
		}

		inline void SetStatus(CoroutineState status) { _status = status; }
		inline CoroutineState GetStatus() const { return _status; }

		inline Processer * GetProcesser() const { return _processer; }
		inline void SetProcesser(Processer * processer) { _processer = processer; }

		inline bool InQueue() const { return _inQueue; }
		inline void SetQueue(bool val) { _inQueue = val; }

		inline void * GetTemp() const { return _temp; }
		inline void SetTemp(void * p) { _temp = p; }

		inline bool TestComplete() { return _flag.load(std::memory_order_acquire) == 0; }
		inline void Acquire() { _flag.store(1, std::memory_order_relaxed); }
		inline void Release() { _flag.store(0, std::memory_order_release); }

	private:
		CoFuncType _fn;
		CoroutineState _status;
		fcontext_t _ctx;

		Processer * _processer;
		bool _inQueue;
		bool _running;

		void * _p;
		int32_t _stackSize;
		void * _temp;

		std::atomic<int32_t> _flag;
	};
}

#endif // !__COROUTINE_H__
