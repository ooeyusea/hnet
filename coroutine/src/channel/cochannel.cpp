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

	ChannelImpl::~ChannelImpl() {
		if (_buffer)
			free(_buffer);

		if (_head) {
			Link * p = _head;
			while (p) {
				Link * c = p;
				p = p->next;

				free(c);
			}
		}
	}

	void ChannelImpl::Push(const void * p) {
		if (_capacity > 0) {
			while (true) {
				std::unique_lock<spin_mutex> lock(_mutex);
				if (_write - _read < _capacity) {
					char * dst = _buffer + (_write % _capacity) * _blockSize;
					SafeMemcpy(dst, _blockSize, p, _blockSize);
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
			SafeMemcpy((char*)link + sizeof(Link), _blockSize, p, _blockSize);

			std::unique_lock<spin_mutex> lock(_mutex);
			link->next = _head;
			_head = link;
			if (_tail == nullptr)
				_tail = link;
		}
	}

	void ChannelImpl::Pop(void * p) {
		if (_capacity > 0) {
			while (true) {
				std::unique_lock<spin_mutex> lock(_mutex);
				if (_write - _read > 0) {
					char * src = _buffer + (_read % _capacity) * _blockSize;
					SafeMemcpy(p, _blockSize, src, _blockSize);
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

					SafeMemcpy(p, _blockSize, (char*)link + sizeof(Link), _blockSize);
					free(link);
				}
			}
		}
	}

	bool ChannelImpl::TryPush(const void * p) {
		if (_capacity > 0) {
			std::unique_lock<spin_mutex> lock(_mutex);
			if (_write - _read < _capacity) {
				char * dst = _buffer + (_write % _capacity) * _blockSize;
				SafeMemcpy(dst, _blockSize, p, _blockSize);
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
			Link * link = (Link *)malloc(sizeof(Link) + _blockSize);
			link->prev = nullptr;
			SafeMemcpy((char*)link + sizeof(Link), _blockSize, p, _blockSize);

			std::unique_lock<spin_mutex> lock(_mutex);
			link->next = _head;
			_head = link;
			if (_tail == nullptr)
				_tail = link;

			return true;
		}
	}

	bool ChannelImpl::TryPop(void * p) {
		if (_capacity > 0) {
			std::unique_lock<spin_mutex> lock(_mutex);
			if (_write - _read > 0) {
				char * src = _buffer + (_read % _capacity) * _blockSize;
				SafeMemcpy(p, _blockSize, src, _blockSize);
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

				SafeMemcpy(p, _blockSize, (char*)link + sizeof(Link), _blockSize);
				free(link);

				return true;
			}
		}
	}

	Channel::Channel(int32_t blockSize, int32_t capacity) {
		_impl = new ChannelImpl(blockSize, capacity);
	}

	Channel::~Channel() {
		delete _impl;
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
}