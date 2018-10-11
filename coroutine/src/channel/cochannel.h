#ifndef __COCHANNEL_H__
#define __COCHANNEL_H__
#include "util.h"
#include "spin_mutex.h"
#include "coroutine.h"

namespace hyper_net {
	class ChannelImpl {
		struct Link {
			Link * prev;
			Link * next;
		};
	public:
		ChannelImpl(int32_t blockSize, int32_t capacity);
		~ChannelImpl();

		void Push(const void * p);
		void Pop(void * p);

		bool TryPush(const void * p);
		bool TryPop(void * p);

		void Close();

	private:
		spin_mutex _mutex;
		int32_t _capacity;
		int32_t _blockSize;
		bool _close = false;

		char * _buffer = nullptr;
		uint32_t _read = 0;
		uint32_t _write = 0;

		Link * _head = nullptr;
		Link * _tail = nullptr;

		std::list<Coroutine*> _writeQueue;
		std::list<Coroutine*> _readQueue;
	};
}

#endif // !__COCHANNEL_H__
