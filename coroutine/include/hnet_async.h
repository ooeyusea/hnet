#ifndef __HNET_ASYNC_H__
#define __HNET_ASYNC_H__

namespace hyper_net {
	class IAsyncQueue {
	public:
		virtual ~IAsyncQueue() {}

		virtual void Call(uint32_t threadId, void * data) = 0;
		virtual void Shutdown() = 0;
	};

	IAsyncQueue * CreateAsyncQueue(int32_t threadCount, bool complete, const std::function<void(void * data)>& f);
}

#define hn_create_async hyper_net::CreateAsyncQueue

#endif // !__HNET_ASYNC_H__
