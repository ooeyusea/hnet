#ifndef __HNET_H__
#define __HNET_H__
#include <functional>
#include <exception>
#include <type_traits>
#include <memory>

namespace hyper_net {
	struct Forker {
		Forker& operator-(const std::function<void()>& f);
		Forker& operator-(int32_t size);

		int32_t stackSize = 0;
	};

	struct Yielder {
		void Do();
	};
}

#define hn_fork hyper_net::Forker()-
#define hn_stack(size) (size)-
#define hn_yield hyper_net::Yielder().Do()

#include "hnet_stream.h"
#include "hnet_net.h"
#include "hnet_time.h"
#include "hnet_channel.h"
#include "hnet_mutex.h"
#include "hnet_async.h"
#include "hnet_rpc.h"
#include "hnet_logger.h"
#endif // !__HNET_H__
