#include "cochannel.h"
#include "scheduler.h"
#include "hnet.h"

namespace hyper_net {
	ChannelImpl::ChannelImpl(int32_t blockSize, int32_t capacity) {
		_capacity = capacity;
		_blockSize = blockSize;
		if (capacity > 0) {
			_buffer = (char*)malloc(blockSize * capacity);
		}
	}

	ChannelImpl::ChannelImpl(int32_t blockSize, int32_t capacity, const std::function<void(void * dst, const void * p)>& pushFn, const std::function<void(void * src, void * p)>& popFn) 
		: _pushFn(pushFn)
		, _popFn(popFn) {
		_capacity = capacity;
		_blockSize = blockSize;
		if (capacity > 0) {
			_buffer = (char*)malloc(blockSize * capacity);
		}
	}

	ChannelImpl::~ChannelImpl() {
		if (_buffer) {
			if (_recoverFn) {
				while (_read < _write) {
					char * src = _buffer + (_read % _capacity) * _blockSize;
					_recoverFn(src);
				}
			}
			free(_buffer);
		}

		if (_head) {
			Link * p = _head;
			while (p) {
				Link * c = p;
				p = p->next;

				if (_recoverFn) {
					char * src = (char*)c + sizeof(Link);
					_recoverFn(src);
				}

				free(c);
			}
		}
	}

	void ChannelImpl::Push(const void * p) {
		if (_capacity > 0) {
			while (true) {
				std::unique_lock<spin_mutex> lock(_mutex);
				if (_close)
					throw ChannelCloseException();

				if (_write - _read < _capacity) {
					char * dst = _buffer + (_write % _capacity) * _blockSize;
					if (!_pushFn)
						SafeMemcpy(dst, _blockSize, p, _blockSize);
					else
						_pushFn(dst, p);
					++_write;

					if (!_readQueue.empty()) {
						Coroutine * co = _readQueue.front();
						_readQueue.pop_front();

						lock.unlock();

						Scheduler::Instance().AddCoroutine(co);
					}
					return;
				}
				else {
					Coroutine * co = Scheduler::Instance().CurrentCoroutine();
					OASSERT(co, "must call in coroutine");

					co->SetStatus(CoroutineState::CS_BLOCK);
					_writeQueue.push_back(co);
					lock.unlock();

					hn_yield;
				}
			}
		}
		else {
			Link * link = (Link *)malloc(sizeof(Link) + _blockSize);
			link->prev = nullptr;
			char * dst = (char*)link + sizeof(Link);
			if (!_pushFn)
				SafeMemcpy(dst, _blockSize, p, _blockSize);
			else
				_pushFn(dst, p);

			std::unique_lock<spin_mutex> lock(_mutex);
			if (_close) {
				if (_recoverFn)
					_recoverFn(dst);
				throw ChannelCloseException();
			}
			link->next = _head;
			if (_head)
				_head->prev = link;
			_head = link;
			if (_tail == nullptr)
				_tail = link;

			if (!_readQueue.empty()) {
				Coroutine * co = _readQueue.front();
				_readQueue.pop_front();

				lock.unlock();

				Scheduler::Instance().AddCoroutine(co);
			}
		}
	}

	void ChannelImpl::Pop(void * p) {
		if (_capacity > 0) {
			while (true) {
				std::unique_lock<spin_mutex> lock(_mutex);
				if (_close)
					throw ChannelCloseException();

				if (_write - _read > 0) {
					char * src = _buffer + (_read % _capacity) * _blockSize;
					if (!_popFn)
						SafeMemcpy(p, _blockSize, src, _blockSize);
					else
						_popFn(src, p);
					++_read;

					if (!_writeQueue.empty()) {
						Coroutine * co = _writeQueue.front();
						_writeQueue.pop_front();

						lock.unlock();

						Scheduler::Instance().AddCoroutine(co);
					}
					return;
				}
				else {
					Coroutine * co = Scheduler::Instance().CurrentCoroutine();
					OASSERT(co, "must call in coroutine");

					co->SetStatus(CoroutineState::CS_BLOCK);
					_readQueue.push_back(co);
					lock.unlock();

					hn_yield;
				}
			}
		}
		else {
			while (true) {
				std::unique_lock<spin_mutex> lock(_mutex);
				if (_close)
					throw ChannelCloseException();

				if (_tail == nullptr) {
					Coroutine * co = Scheduler::Instance().CurrentCoroutine();
					OASSERT(co, "must call in coroutine");

					co->SetStatus(CoroutineState::CS_BLOCK);
					_readQueue.push_back(co);
					lock.unlock();

					hn_yield;
				}
				else {
					Link * link = _tail;
					if (link->prev) {
						link->prev->next = nullptr;
						_tail = link->prev;
					}
					else
						_head = _tail = nullptr;

					lock.unlock();

					char * src = (char*)link + sizeof(Link);
					if (!_popFn)
						SafeMemcpy(p, _blockSize, src, _blockSize);
					else
						_popFn(src, p);
					free(link);
					break;
				}
			}
		}
	}

	bool ChannelImpl::TryPush(const void * p) {
		if (_capacity > 0) {
			std::unique_lock<spin_mutex> lock(_mutex);
			if (_close)
				return false;

			if (_write - _read < _capacity) {
				char * dst = _buffer + (_write % _capacity) * _blockSize;
				if (!_pushFn)
					SafeMemcpy(dst, _blockSize, p, _blockSize);
				else
					_pushFn(dst, p);
				++_write;

				if (!_readQueue.empty()) {
					Coroutine * co = _readQueue.front();
					_readQueue.pop_front();

					lock.unlock();

					Scheduler::Instance().AddCoroutine(co);
				}
				return true;
			}
			else
				return false;
		}
		else {
			if (_close)
				return false;

			Link * link = (Link *)malloc(sizeof(Link) + _blockSize);
			link->prev = nullptr;
			char * dst = (char*)link + sizeof(Link);
			if (!_pushFn)
				SafeMemcpy(dst, _blockSize, p, _blockSize);
			else
				_pushFn(dst, p);

			std::unique_lock<spin_mutex> lock(_mutex);
			link->next = _head;
			if (_head)
				_head->prev = link;
			_head = link;
			if (_tail == nullptr)
				_tail = link;

			if (!_readQueue.empty()) {
				Coroutine * co = _readQueue.front();
				_readQueue.pop_front();

				lock.unlock();

				Scheduler::Instance().AddCoroutine(co);
			}
			return true;
		}
	}

	bool ChannelImpl::TryPop(void * p) {
		if (_capacity > 0) {
			std::unique_lock<spin_mutex> lock(_mutex);
			if (_close)
				return false;

			if (_write - _read > 0) {
				char * src = _buffer + (_read % _capacity) * _blockSize;
				if (!_popFn)
					SafeMemcpy(p, _blockSize, src, _blockSize);
				else
					_popFn(src, p);
				++_read;

				if (!_writeQueue.empty()) {
					Coroutine * co = _writeQueue.front();
					_writeQueue.pop_front();

					lock.unlock();

					Scheduler::Instance().AddCoroutine(co);
				}
				return true;
			}
			else
				return false;
		}
		else {
			std::unique_lock<spin_mutex> lock(_mutex);
			if (_close)
				return false;

			if (_tail == nullptr)
				return false;
			else {
				Link * link = _tail;
				if (link->prev) {
					link->prev->next = nullptr;
					_tail = link->prev;
				}
				else
					_head = _tail = nullptr;

				lock.unlock();

				char * src = (char*)link + sizeof(Link);
				if (!_popFn)
					SafeMemcpy(p, _blockSize, src, _blockSize);
				else
					_popFn(src, p);
				free(link);

				return true;
			}
		}
	}

	void ChannelImpl::Close() {
		std::unique_lock<spin_mutex> lock(_mutex);
		_close = true;

		std::list<Coroutine*> writeQueue;
		writeQueue.swap(_writeQueue);

		std::list<Coroutine*> readQueue;
		readQueue.swap(_readQueue);

		lock.unlock();

		for (auto * co : writeQueue)
			Scheduler::Instance().AddCoroutine(co);

		for (auto * co : readQueue)
			Scheduler::Instance().AddCoroutine(co);
	}

	Channel::Channel(int32_t blockSize, int32_t capacity) {
		_impl = new ChannelImpl(blockSize, capacity);
	}

	Channel::~Channel() {
		delete _impl;
	}

	void Channel::SetFn(const std::function<void(void * dst, const void * p)>& pushFn, const std::function<void(void * src, void * p)>& popFn, const std::function<void(void * src)>& recoverFn) {
		_impl->SetFn(pushFn, popFn, recoverFn);
	}

	void Channel::Push(const void * p) {
		_impl->Push(p);
	}

	void Channel::Pop(void * p) {
		_impl->Pop(p);
	}

	bool Channel::TryPush(const void * p) {
		return _impl->TryPush(p);
	}

	bool Channel::TryPop(void * p) {
		return _impl->TryPop(p);
	}

	void Channel::Close() {
		_impl->Close();
	}
}