#ifndef __HNET_H__
#define __HNET_H__
#include <functional>
#include <exception>
#include <type_traits>
#include <memory>

namespace hyper_net {
	typedef void * coroutine_t;

	struct Forker {
		Forker& operator-(const std::function<void()>& f);
		Forker& operator-(int32_t size);

		int32_t stackSize = 0;
	};

	struct Yielder {
		coroutine_t Current();
		void SetTemp(void * p);
		void * GetTemp(coroutine_t co);
		void Do(bool block);
		void Resume(coroutine_t co);
	};
}

#define hn_fork hyper_net::Forker()-
#define hn_stack(size) (size)-

#define hn_co hyper_net::coroutine_t
#define hn_current hyper_net::Yielder().Current()
#define hn_set hyper_net::Yielder().SetTemp
#define hn_get hyper_net::Yielder().GetTemp
#define hn_yield hyper_net::Yielder().Do(false)
#define hn_block hyper_net::Yielder().Do(true)
#define hn_resume hyper_net::Yielder().Resume

#include "hnet_stream.h"
#include "hnet_net.h"
#include "hnet_time.h"
#include "hnet_channel.h"
#include "hnet_mutex.h"
#include "hnet_async.h"
#include "hnet_rpc.h"
#include "hnet_logger.h"
#endif // !__HNET_H__
