#ifndef __COROUTINE_H__
#define __COROUTINE_H__
#include "util.h"
#ifdef USE_FCONTEXT
#include <boost/context/detail/fcontext.hpp>
using boost::context::detail::fcontext_t;
#endif

namespace hyper_net {
	enum class CoroutineState {
		CS_RUNNABLE,
		CS_BLOCK,
		CS_DONE,
	};

#ifndef USE_FCONTEXT
	typedef void * fcontext_t;
#else
#endif

	typedef std::function<void()> CoFuncType;

	class Processer;
	class Coroutine {
		Coroutine(Coroutine&) = delete;
		Coroutine(const Coroutine&) = delete;

		Coroutine& operator=(Coroutine&) = delete;
		Coroutine& operator=(const Coroutine&) = delete;

	public:
		Coroutine();
		Coroutine(const CoFuncType& f);
		Coroutine(Coroutine&& rhs);
		~Coroutine();

		Coroutine& operator=(Coroutine&& rhs);

		void Run();
		void SwapIn();
		void SwapTo(Coroutine & co);
		void SwapOut();

		static void CoroutineProc(void * p);

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

		inline void * GetTemp() const { return _p; }
		inline void SetTemp(void * p) { _p = p; }

	private:
		CoFuncType _fn;
		CoroutineState _status;
		fcontext_t _ctx;

		Processer * _processer;
		bool _inQueue;

		void * _p;
	};
}

#endif // !__COROUTINE_H__