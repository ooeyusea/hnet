#ifndef __SLIST_H__
#define __SLIST_H__
#include "util.h"
#include "spin_mutex.h"

namespace hyper_net {
	template <typename T>
	struct LinkNode {
		T * next;
		T * prev;
	};

	template <typename T>
	class SList {
		static_assert((std::is_base_of<LinkNode<T>, T>::value), "T must be baseof TSQueueHook");

	public:
		SList() : _head(nullptr), _tail(nullptr) {}
		~SList() {
			OASSERT(Empty(), "slist must recover without node");
		}

		SList(const SList&) = delete;
		SList& operator=(const SList&) = delete;

		SList(SList&& rhs) {
			_head = rhs._head;
			_tail = rhs._tail;
			_count = rhs._count;

			rhs.Steal();
		}

		inline SList& operator=(SList&& rhs) {
			_head = rhs._head;
			_tail = rhs._tail;
			_count = rhs._count;

			rhs.Steal();
			return *this;
		}

		inline void Append(SList&& rhs) {
			if (rhs.Empty())
				return;

			if (Empty()) {
				*this = std::move(rhs);
				return;
			}

			_tail->next = rhs._head;
			rhs._head->prev = _tail;

			_tail = rhs._tail;
			_count += rhs._count;

			rhs.Steal();
		}

		inline bool Empty() const {
			return _count == 0;
		}

		inline void Steal() {
			_head = _tail = nullptr;
			_count = 0;
		}

		inline T * Head() { return _head; }
		inline T * Tail() { return _tail; }

	private:
		T * _head;
		T * _tail;
		int32_t _count;
	};

	template <typename T>
	class TsQueue {
	public:
		TsQueue() {
			_head = _tail = nullptr;
			_count = 0;
		}

		~TsQueue() {
			Clear();
		}

		

	private:
		spin_mutex _lock;

		T * _head;
		T * _tail;
		int32_t _count;
	};
}

#endif // !__SLIST_H__
