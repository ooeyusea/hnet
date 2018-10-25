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
		ChannelImpl(int32_t blockSize, int32_t capacity, const std::function<void(void * dst, const void * p)>& pushFn, const std::function<void(void * src, void * p)>& popFn);
		~ChannelImpl();

		inline void SetFn(const std::function<void(void * dst, const void * p)>& pushFn, const std::function<void(void * src, void * p)>& popFn, const std::function<void(void * src)>& recoverFn) {
			_pushFn = pushFn;
			_popFn = popFn;
			_recoverFn = recoverFn;
		}

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

		std::function<void(void * dst, const void * p)> _pushFn;
		std::function<void(void * src, void * p)> _popFn;
		std::function<void(void * src)> _recoverFn;
	};
}

#endif // !__COCHANNEL_H__
