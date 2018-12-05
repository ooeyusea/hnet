#include "corpc.h"
#include "scheduler.h"
#include "lock_free_list.h"

#define MAX_PACKET_SIZE 65536
#define RPC_TIMEOUT 5000
#define MAX_SERVICE_SIZE 997
#define RPC_LINE_TIMEOUT 30000
#define RPC_LINE_CHECK_INTERVAL 5000
#define RPC_LINE_HEARBEAT_INTERVAL 15000
#define MAX_RPC_SZIE 9941
#define MAX_TEST_COUNT 15
#define CHECK_TIMEOUT_INTERVAL 100
#define RPC_ORDER_TIMEOUT 15000
#define RPC_ORDER_CHECK_INTERVAL 50

namespace hyper_net {
#pragma pack(push, 1)
	struct RpcHeader {
		int16_t size;
		int8_t op;
		uint64_t seq;

		enum {
			OP_PING = 0,
			OP_PONG,
			OP_REQUEST,
			OP_RESPOND,
			OP_RESPOND_NO_FUNC,
			OP_RESPOND_FAIL,
			OP_RESPOND_TIMEOUT,
		};
	};
#pragma pack(pop)

	struct RpcDeliverBuff {
		mutable char * data = nullptr;
		mutable int32_t size = 0;

		void Assign(const char * buff, int32_t len) {
			if (len > 0) {
				data = (char*)malloc(len);
				memcpy(data, buff, len);
				size = len;
			}
		}

		void Release() const {
			if (data) {
				free(data);
				data = nullptr;
				size = 0;
			}
		}
	};

	class RpcLineBrokenException : RpcException {
	public:
		virtual char const* what() const noexcept { return "rpc pipe broken"; }
	};

	class RpcTooLargePacketException : RpcException {
	public:
		virtual char const* what() const noexcept { return "rpc too large packet"; }
	};

	class RpcDecodeFailedException : RpcException {
	public:
		virtual char const* what() const noexcept { return "rpc decode failed"; }
	};

	struct RpcJustCall {
		char * dst = nullptr;
		int32_t size = 0;
		bool success = false;
	};

	class RpcRetImpl {
	public:
		RpcRetImpl() {}
		~RpcRetImpl() {}

		inline void BindSequence(int32_t * fd, uint64_t seq) {
			_fd = fd;
			_seq = seq;
		}

		inline void BindCoroutine(Coroutine * co) {
			_co = co;
		}

		inline void Ret(const void * context, int32_t size) {
			if (_fd != nullptr) {
				char msg[MAX_PACKET_SIZE];
				RpcHeader& header = *(RpcHeader*)msg;
				header.op = RpcHeader::OP_RESPOND;
				header.seq = _seq;
				header.size = (int16_t)(sizeof(RpcHeader) + size);

				if (header.size >= MAX_PACKET_SIZE)
					throw RpcTooLargePacketException();

				SafeMemcpy(msg + sizeof(RpcHeader), MAX_PACKET_SIZE - sizeof(RpcHeader), context, size);

				hn_send(*_fd, msg, header.size);
			}
			else if (_co) {
				RpcJustCall * rst = (RpcJustCall *)_co->GetTemp();
				rst->success = true;
				if (rst->dst) {
					SafeMemcpy(rst->dst, rst->size, context, size);
					rst->size = (rst->size > size ? size : rst->size);
				}

				Scheduler::Instance().AddCoroutine(_co);
			}
		}

		inline void Fail() {
			if (_fd != nullptr) {
				RpcHeader header;
				header.op = RpcHeader::OP_RESPOND_FAIL;
				header.seq = _seq;
				header.size = sizeof(RpcHeader);

				hn_send(*_fd, (const char*)&header, header.size);
			}
		}

	private:
		int32_t * _fd = nullptr;
		uint64_t _seq = 0;
		Coroutine * _co = nullptr;
	};

	RpcRet::RpcRet() {
		_impl = new RpcRetImpl;
	}

	RpcRet::~RpcRet() {
		delete _impl;
	}


	void RpcRet::Ret(const void * context, int32_t size) {
		_impl->Ret(context, size);
	}

	void RpcRet::Fail() {
		_impl->Fail();
	}

	class RpcImpl {
		struct WaitState {
			Coroutine * co;

			int8_t state;
			char * msg;
			int16_t size;
		};

		struct Service {
			long serviceId = 0;
			int32_t fd = 0;
		};

		struct TimeoutCheck {
			int32_t idx;
			uint64_t state;
			int64_t util;

			AtomicIntrusiveLinkedListHook<TimeoutCheck> next;
		};

		struct OrderRpc {
			const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn;
			RpcDeliverBuff buf;
			int32_t * fd;
			uint64_t seq;
			Coroutine * co;
		};

		struct RpcFunction {
			std::function<int64_t (const void * context, int32_t size)> orderFn;
			std::function<void(const void * context, int32_t size, RpcRet & ret)> callFn;
		};

	public:
		RpcImpl() {
			SafeMemset(_waits, sizeof(_waits), 0, sizeof(_waits));

			_clearThread = std::thread(&RpcImpl::DoClearTimeout, this);
		}

		~RpcImpl() {
			_terminate = true;
			_clearThread.join();
		}

		inline Service& SetupServiceFd(uint32_t serviceId, int32_t fd) {
			int32_t count = 0;
			while (count < MAX_SERVICE_SIZE) {
				Service& service = _services[(serviceId + count) % MAX_SERVICE_SIZE];
				++count;

				if (service.serviceId == serviceId) {
					service.fd = fd;
					return service;
				}
#ifdef WIN32
				else if (InterlockedCompareExchangeNoFence(&service.serviceId, serviceId, 0) == 0) {
#else
				else if (__sync_bool_compare_and_swap(&service.serviceId, 0, serviceId)) {
#endif
					service.fd = fd;
					return service;
				}
			}
			return _null;
		}

		inline void Start(uint32_t serviceId, int32_t fd, const char * context, int32_t size) {
			Service& service = FindService(serviceId);
			if (&service == &_null || service.fd != fd)
				return;

			char msg[MAX_PACKET_SIZE];
			int32_t offset = 0;

			if (size > 0) {
				offset = size;
				int32_t pos = 0;
				while (offset - pos >= sizeof(RpcHeader)) {
					RpcHeader& header = *(RpcHeader*)(context + pos);
					if (offset - pos < header.size) {
						break;
					}

					if (header.size > MAX_PACKET_SIZE) {
						hn_close(fd);
						return;
					}

					if (header.op == RpcHeader::OP_PING) {
						header.op = RpcHeader::OP_PONG;
						hn_send(fd, (const char*)&header, sizeof(header));
					}
					else if (header.op == RpcHeader::OP_REQUEST) {
						DoRequest(service, header.seq, context + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
					}
					else if (header.op == RpcHeader::OP_RESPOND)
						DoRespond(header.seq, context + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
					else
						DoRespondFail(header.seq, header.op);

					pos += header.size;
				}

				if (offset > pos) {
					SafeMemcpy(msg, MAX_PACKET_SIZE, context + pos, offset - pos);
					offset -= pos;
				}
				else
					offset = 0;
			}

			bool stop = false;
			int64_t activeTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			hn_channel<int8_t, 1> ch;

			hn_fork [ch, &stop, &activeTick, fd, this]{
				int64_t sendTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				while (!stop && !_terminate) {
					hn_sleep RPC_LINE_CHECK_INTERVAL;

					int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					if (now - activeTick > RPC_LINE_TIMEOUT) {
						break;
					}

					if (now - sendTick >= RPC_LINE_HEARBEAT_INTERVAL) {
						RpcHeader header;
						header.size = sizeof(header);
						header.op = RpcHeader::OP_PING;

						hn_send(fd, (const char*)&header, sizeof(header));
					}
				}

				hn_shutdown(fd);
				ch << (int8_t)1;
			};

			while (true) {
				int32_t len = hn_recv(fd, msg + offset, MAX_PACKET_SIZE - offset);
				if (len < 0) {
					hn_close(fd);
					break;
				}

				offset += len;

				bool invalid = false;
				int32_t pos = 0;
				while (offset - pos >= sizeof(RpcHeader)) {
					RpcHeader& header = *(RpcHeader*)(msg + pos);
					if (offset - pos < header.size) {
						break;
					}

					if (header.size > MAX_PACKET_SIZE) {
						invalid = true;
						break;
					}

					if (header.op == RpcHeader::OP_PING) {
						header.op = RpcHeader::OP_PONG;
						hn_send(fd, (const char*)&header, sizeof(header));
					}
					else if (header.op == RpcHeader::OP_PONG) {
						activeTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					}
					else if (header.op == RpcHeader::OP_REQUEST) {
						DoRequest(service, header.seq, msg + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
					}
					else if (header.op == RpcHeader::OP_RESPOND) {
						DoRespond(header.seq, msg + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
					}
					else
						DoRespondFail(header.seq, header.op);

					pos += header.size;
				}

				if (invalid)
					break;

				if (offset > pos) {
					::memmove(msg, msg + pos, offset - pos);
					offset -= pos;
				}
				else
					offset = 0;
			}

			stop = true;

			int8_t res = 0;
			ch >> res;
		}

		inline void RegisterFn(int32_t rpcId, const std::function<int64_t(const void * context, int32_t size)>& order, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn) {
			_cbs[rpcId] = { order, fn };
		}

		inline void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size) {
			if (serviceId == JUST_CALL) {
				JustDoRequest(rpcId, (const char *)context, size);
			}
			else {
				Service& service = FindService(serviceId);
				if (&service == &_null)
					throw RpcLineBrokenException();

				char msg[MAX_PACKET_SIZE];
				RpcHeader& header = *(RpcHeader*)msg;
				header.op = RpcHeader::OP_REQUEST;
				header.seq = 0;
				header.size = (int16_t)(sizeof(RpcHeader) + sizeof(rpcId) + size);

				if (header.size >= MAX_PACKET_SIZE)
					throw RpcTooLargePacketException();

				*(int32_t*)(msg + sizeof(RpcHeader)) = rpcId;
				SafeMemcpy(msg + sizeof(RpcHeader) + sizeof(int32_t), MAX_PACKET_SIZE - sizeof(RpcHeader) - sizeof(int32_t), context, size);

				hn_send(service.fd, msg, header.size);
			}
		}

		inline void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn) {
			char msg[MAX_PACKET_SIZE];
			if (serviceId == JUST_CALL) {
				int32_t ret = JustDoRequest(rpcId, (const char *)context, size, msg, MAX_PACKET_SIZE);
				if (ret < 0)
					throw RpcException();

				if (!fn(msg, ret))
					throw RpcDecodeFailedException();
			}
			else {
				Coroutine * co = Scheduler::Instance().CurrentCoroutine();
				OASSERT(co, "must in co");

				Service& service = FindService(serviceId);
				if (&service == &_null)
					throw RpcLineBrokenException();
				
				WaitState state{ co, 0, msg, 0 };

				RpcHeader& header = *(RpcHeader*)msg;
				header.op = RpcHeader::OP_REQUEST;
				header.size = (int16_t)(sizeof(RpcHeader) + sizeof(rpcId) + size);

				if (header.size >= MAX_PACKET_SIZE)
					throw RpcTooLargePacketException();

				header.seq = SetupWait(&state, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + RPC_TIMEOUT);

				if (header.seq == 0)
					throw RpcException();
				else {
					*(int32_t*)(msg + sizeof(RpcHeader)) = rpcId;
					SafeMemcpy(msg + sizeof(RpcHeader) + sizeof(int32_t), MAX_PACKET_SIZE - sizeof(RpcHeader) - sizeof(int32_t), context, size);

					hn_send(service.fd, msg, header.size);

					co->SetStatus(CoroutineState::CS_BLOCK);
					hn_yield;

					if (state.state != 0)
						throw RpcException();

					if (!fn(msg, state.size))
						throw RpcDecodeFailedException();
				}
			}
		}

		inline void StartOrder(int64_t order, hn_channel<OrderRpc*> channel) {
			hn_fork[order, channel, this]{
				int64_t last = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				while (true) {
					int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

					OrderRpc * rpc;
					if (!channel.TryPop(rpc)) {
						if (now - last > RPC_ORDER_TIMEOUT) {
							std::unique_lock<spin_mutex> lock(_mutex);
							if (!channel.TryPop(rpc)) {
								_orders.erase(order);
								return;
							}
						}
						else {
							hn_sleep RPC_ORDER_CHECK_INTERVAL;
							continue;
						}
					};

					last = now;

					RpcRet ret;
					if (rpc->seq != 0)
						ret._impl->BindSequence(rpc->fd, rpc->seq);
					else if (rpc->co)
						ret._impl->BindCoroutine(rpc->co);

					rpc->fn(rpc->buf.data, rpc->buf.size, ret);

					delete rpc;
				}
			};
		}

		int32_t JustDoRequest(int32_t rpcId, const char * context, int32_t size, char * dst = nullptr, int32_t maxSize = 0) {
			auto itr = _cbs.find(rpcId);
			if (itr != _cbs.end()) {
				auto & cb = itr->second;

				int64_t order = 0;
				if (cb.orderFn)
					order = cb.orderFn(context, size);
			
				RpcDeliverBuff buf;
				buf.Assign(context, size);
			
				Coroutine * co = Scheduler::Instance().CurrentCoroutine();
				OASSERT(co, "must call in coroutine");

				RpcJustCall rst{ dst, maxSize };
				co->SetTemp(&rst);

				if (order == 0) {
					hn_fork[this, co, &cb, buf]() {
						RpcRet ret;
						ret._impl->BindCoroutine(co);
			
						cb.callFn(buf.data, (int32_t)buf.size, ret);
			
						buf.Release();
					};
				}
				else {
					std::unique_lock<spin_mutex> lock(_mutex);
					auto itr = _orders.find(order);
					if (itr != _orders.end()) {
						itr->second << new OrderRpc{ cb.callFn, buf, nullptr, 0,  co };
					}
					else {
						auto ret = _orders.insert(std::make_pair(order, hn_channel<OrderRpc*>()));
						itr = ret.first;
			
						itr->second << new OrderRpc{ cb.callFn, buf, nullptr, 0,  co };
						lock.unlock();
			
						StartOrder(order, itr->second);
					}
				}

				co->SetStatus(CoroutineState::CS_BLOCK);
				hn_yield;

				if (rst.success)
					return rst.size;
			}
			return -1;
		}

		void DoRequest(Service& service, uint64_t seq, const char * context, int32_t size) {
			int32_t rpcId = *(int32_t*)context;

			auto itr = _cbs.find(rpcId);
			if (itr != _cbs.end()) {
				auto & cb = itr->second;

				int64_t order = 0;
				if (cb.orderFn)
					order = cb.orderFn(context, size);

				RpcDeliverBuff buf;
				buf.Assign(context + sizeof(int32_t), size - sizeof(int32_t));

				if (order == 0) {
					hn_fork[this, &service, seq, &cb, buf]() {
						RpcRet ret;
						if (seq != 0)
							ret._impl->BindSequence(&service.fd, seq);

						cb.callFn(buf.data, (int32_t)buf.size, ret);

						buf.Release();
					};
				}
				else {
					std::unique_lock<spin_mutex> lock(_mutex);
					auto itr = _orders.find(order);
					if (itr != _orders.end()) {
						itr->second << new OrderRpc{ cb.callFn, buf, &(service.fd), seq };
					}
					else {
						auto ret = _orders.insert(std::make_pair(order, hn_channel<OrderRpc*>()));
						itr = ret.first;

						itr->second << new OrderRpc{ cb.callFn, buf, &(service.fd), seq };
						lock.unlock();

						StartOrder(order, itr->second);
					}		
				}
			}
			else if (seq != 0) {
				RpcHeader header;
				header.op = RpcHeader::OP_RESPOND_NO_FUNC;
				header.seq = 0;
				header.size = (int16_t)(sizeof(RpcHeader) + sizeof(rpcId) + size);

				hn_send(service.fd, (const char*)&header, sizeof(header));
			}
		}

		void DoRespond(uint64_t seq, const char * context, int32_t size) {
			WaitState * state = FindWait(seq);
			if (state != nullptr) {
				state->state = 0;

				SafeMemcpy(state->msg, size, context, size);
				state->size = size;

				Scheduler::Instance().AddCoroutine(state->co);
			}
		}

		void DoRespondFail(uint64_t seq, int8_t op) {
			WaitState * state = FindWait(seq);
			if (state != nullptr) {
				state->state = op;
				
				Scheduler::Instance().AddCoroutine(state->co);
			}
		}

		void DoClearTimeout() {
			std::multimap<int64_t, TimeoutCheck * > _checks;
			while (!_terminate) {
				_waitQueue.SweepOnce([&_checks](TimeoutCheck * u) {
					_checks.insert(std::make_pair(u->util, u));
				});

				int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

				auto itr = _checks.begin();
				while (itr != _checks.end()) {
					auto check = itr;
					++itr;

					if (check->first <= now) {
						uint64_t state = _waits[check->second->idx];
						if (state == check->second->state) {
#ifdef WIN32
							if (InterlockedCompareExchangeNoFence(&_waits[check->second->idx], 0, state) == state) {
#else
							if (__sync_bool_compare_and_swap(&_waits[check->second->idx], state, 0)) {
#endif
								WaitState& state = (*(WaitState*)check->second->state);
								state.state = RpcHeader::OP_RESPOND_TIMEOUT;

								Scheduler::Instance().AddCoroutine(state.co);
							}
						}

						delete check->second;
						_checks.erase(check);
					}
					else
						break;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_TIMEOUT_INTERVAL));
			}
		}
	
		inline Service& FindService(uint32_t serviceId) {
			int32_t count = 0;
			while (count < MAX_SERVICE_SIZE) {
				Service& service = _services[(serviceId + count) % MAX_SERVICE_SIZE];
				++count;

				if (service.serviceId == serviceId)
					return service;
			}
			return _null;
		}

		uint64_t SetupWait(WaitState * state, int64_t check) {
			uint64_t count = 0;
			while (count < MAX_TEST_COUNT) {
				uint64_t seq = _nextSeq++;
				if (seq == 0)
					seq = _nextSeq++;
#ifdef WIN32
				if (InterlockedCompareExchangeAcquire(&_waits[seq % MAX_RPC_SZIE], (uint64_t)state, 0) == 0) {
#else
				if (__sync_bool_compare_and_swap(&_waits[seq % MAX_RPC_SZIE], 0, (uint64_t)state)) {
#endif
					TimeoutCheck * unit = new TimeoutCheck{ (seq % MAX_RPC_SZIE), (uint64_t)state, check };
					_waitQueue.InsertHead(unit);
					return seq;
				}

				++count;
			}
			return 0;
		}

		inline WaitState * FindWait(uint64_t seq) {
			uint64_t state = _waits[seq % MAX_RPC_SZIE];
#ifdef WIN32
			if (InterlockedCompareExchangeNoFence(&_waits[seq % MAX_RPC_SZIE], 0, state) == state) {
#else
			if (__sync_bool_compare_and_swap(&_waits[seq % MAX_RPC_SZIE], state, 0)) {
#endif
				return (WaitState*)state;
			}
			return nullptr;
		}

	private:
		Service _services[MAX_SERVICE_SIZE];
		Service _null;
		std::unordered_map<int32_t, RpcFunction> _cbs;

		uint64_t _waits[MAX_RPC_SZIE];
		uint64_t _nextSeq = 0;

		std::thread _clearThread;
		bool _terminate = false;
		AtomicIntrusiveLinkedList<TimeoutCheck, &TimeoutCheck::next> _waitQueue;

		spin_mutex _mutex;
		std::unordered_map<int64_t, hn_channel<OrderRpc*>> _orders;
	};

	Rpc::Rpc() {
		_impl = new RpcImpl;
	}

	Rpc::~Rpc() {
		delete _impl;
	}

	void Rpc::Attach(uint32_t serviceId, int32_t fd) {
		_impl->SetupServiceFd(serviceId, fd);
	}

	void Rpc::Start(uint32_t serviceId, int32_t fd, const char * context, int32_t size) {
		_impl->Start(serviceId, fd, context, size);
	}

	void Rpc::RegisterFn(int32_t rpcId, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn) {
		_impl->RegisterFn(rpcId, nullptr, fn);
	}

	void Rpc::RegisterFn(int32_t rpcId, const std::function<int64_t(const void * context, int32_t size)>& order,
		const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn) {
		_impl->RegisterFn(rpcId, order, fn);
	}

	void Rpc::Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size) {
		_impl->Call(serviceId, rpcId, context, size);
	}

	void Rpc::Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn) {
		_impl->Call(serviceId, rpcId, context, size, fn);
	}
}
