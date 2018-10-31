#include "corpc.h"
#include "scheduler.h"

#define MAX_PACKET_SIZE 65536
#define RPC_TIMEOUT 
#define MAX_SERVICE_SIZE 997
#define RPC_LINE_TIMEOUT 30000
#define RPC_LINE_CHECK_INTERVAL 5000
#define RPC_LINE_HEARBEAT_INTERVAL 15000
#define MAX_RPC_SZIE 9941
#define MAX_TEST_COUNT 10

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
		};
	};
#pragma pack(pop)

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

	class RpcRetImpl {
	public:
		RpcRetImpl() {}
		~RpcRetImpl() {}

		inline void BindSequence(int32_t * fd, uint64_t seq) {
			_fd = fd;
			_seq = seq;
		}

		inline void Ret(const void * context, int32_t size) {
			if (_fd != nullptr) {
				char msg[MAX_PACKET_SIZE];
				RpcHeader& header = *(RpcHeader*)msg;
				header.op = RpcHeader::OP_RESPOND;
				header.seq = _seq;
				header.size = sizeof(RpcHeader) + size;

				if (header.size >= MAX_PACKET_SIZE)
					throw RpcTooLargePacketException();

				SafeMemcpy(msg + sizeof(RpcHeader), MAX_PACKET_SIZE - sizeof(RpcHeader), context, size);

				hn_send(*_fd, msg, header.size);
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
			int64_t tick;

			int8_t state;
			char * msg;
			int16_t size;
		};

		struct Service {
			long serviceId = 0;
			int32_t fd = 0;
		};

	public:
		RpcImpl() {
			SafeMemset(_waits, sizeof(_waits), 0, sizeof(_waits));
		}
		~RpcImpl() {}

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
				else if (__sync_bool_compare_and_swap(&service.serviceId, 0, serviceid)) {
#endif
					service.fd = fd;
					return service;
				}
			}
			return _null;
		}

		inline void Attach(uint32_t serviceId, int32_t fd, const void * context, int32_t size) {
			Service& service = SetupServiceFd(serviceId, fd);
			if (&service == &_null)
				return;

			std::string buf;
			if (context != nullptr && size > 0)
				buf.append((const char*)context, size);

			hn_fork [this, &service, fd, buf]() {
				char msg[MAX_PACKET_SIZE];
				int32_t offset = 0;

				if (!buf.empty()) {
					offset = buf.size();
					int32_t pos = 0;
					while (offset - pos >= sizeof(RpcHeader)) {
						RpcHeader& header = *(RpcHeader*)(buf.data() + pos);
						if (offset - pos < header.size) {
							break;
						}

						if (header.size > 0) {
							hn_close(fd);
							return;
						}

						if (header.op == RpcHeader::OP_PING) {
							header.op = RpcHeader::OP_PONG;
							hn_send(fd, (const char*)&header, sizeof(header));
						}
						else if (header.op == RpcHeader::OP_REQUEST) {
							DoRequest(service, header.seq, buf.data() + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
						}
						else if (header.op == RpcHeader::OP_RESPOND)
							DoRespond(header.seq, buf.data() + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
						else
							DoRespondFail(header.seq, header.op);

						pos += header.size;
					}

					if (offset > pos) {
						SafeMemcpy(msg, MAX_PACKET_SIZE, buf.data() + pos, offset - pos);
						offset -= pos;
					}
					else
						offset = 0;
				}

				bool stop = false;
				int64_t activeTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
				hn_channel<int8_t, 1> ch;

				hn_fork [ch, &stop, &activeTick, fd]{
					int64_t sendTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					while (!stop) {
						hn_sleep RPC_LINE_CHECK_INTERVAL;

						int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
						if (now - activeTick > RPC_LINE_TIMEOUT) {
							hn_shutdown(fd);
							break;
						}

						if (now - sendTick >= RPC_LINE_HEARBEAT_INTERVAL) {
							RpcHeader header;
							header.size = sizeof(header.size);
							header.op = RpcHeader::OP_PING;

							hn_send(fd, (const char*)&header, sizeof(header));
						}
					}

					ch << (int8_t)1;
				};

				while (true) {
					int32_t len = hn_recv(fd, msg + offset, MAX_PACKET_SIZE - offset);
					if (len < 0) {
						hn_close(fd);
						break;
					}

					offset += len;

					int32_t pos = 0;
					while (offset - pos >= sizeof(RpcHeader)) {
						RpcHeader& header = *(RpcHeader*)(msg + pos);
						if (offset - pos < header.size) {
							break;
						}

						if (header.size > 0) {
							hn_close(fd);
							return;
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
							DoRespond(header.seq, buf.data() + pos + sizeof(RpcHeader), header.size - sizeof(RpcHeader));
						}
						else
							DoRespondFail(header.seq, header.op);

						pos += header.size;
					}

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
			};
		}

		inline void RegisterFn(int32_t rpcId, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn) {
			_cbs[rpcId] = fn;
		}

		inline void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size) {
			Service& service = FindService(serviceId);
			if (&service == &_null)
				throw RpcLineBrokenException();

			char msg[MAX_PACKET_SIZE];
			RpcHeader& header = *(RpcHeader*)msg;
			header.op = RpcHeader::OP_REQUEST;
			header.seq = 0;
			header.size = sizeof(RpcHeader) + sizeof(rpcId) + size;

			if (header.size >= MAX_PACKET_SIZE)
				throw RpcTooLargePacketException();

			*(int32_t*)(msg + sizeof(RpcHeader)) = rpcId;
			SafeMemcpy(msg + sizeof(RpcHeader) + sizeof(int32_t), MAX_PACKET_SIZE - sizeof(RpcHeader) - sizeof(int32_t), context, size);

			hn_send(service.fd, msg, header.size);
		}

		inline void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn) {
			Coroutine * co = Scheduler::Instance().CurrentCoroutine();
			OASSERT(co, "must in co");

			Service& service = FindService(serviceId);
			if (&service == &_null)
				throw RpcLineBrokenException();

			char msg[MAX_PACKET_SIZE];
			WaitState state{ co, 0, 0, msg, 0 };
			
			RpcHeader& header = *(RpcHeader*)msg;
			header.op = RpcHeader::OP_REQUEST;
			header.seq = (uint64_t)&state;
			header.size = sizeof(RpcHeader) + sizeof(rpcId) + size;

			if (header.size >= MAX_PACKET_SIZE)
				throw RpcTooLargePacketException();

			if (!SetupWait(&state))
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

		void DoRequest(Service& service, uint64_t seq, const char * context, int32_t size) {
			int32_t rpcId = *(int32_t*)context;
			
			auto itr = _cbs.find(rpcId);
			if (itr != _cbs.end()) {
				auto & fn = itr->second;

				std::string buf(context + sizeof(int32_t), size - sizeof(int32_t));
				hn_fork[this, &service, seq, &fn, buf]() {
					RpcRet ret;
					if (seq != 0)
						ret._impl->BindSequence(&service.fd, seq);

					fn(buf.data(), buf.size(), ret);
				};
			}
			else if (seq != 0) {
				RpcHeader header;
				header.op = RpcHeader::OP_RESPOND_NO_FUNC;
				header.seq = 0;
				header.size = sizeof(RpcHeader) + sizeof(rpcId) + size;

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

		bool SetupWait(WaitState * state) {
			uint64_t seq = (uint64_t)state;
			uint64_t count = 0;
			while (count < MAX_TEST_COUNT) {
#ifdef WIN32
				if (InterlockedCompareExchangeAcquire(&_waits[(seq + count) % MAX_RPC_SZIE], (uint64_t)state, 0) == 0) {
#else
				if (__sync_bool_compare_and_swap(&_waits[(seq + count) % MAX_RPC_SZIE], 0, (uint64_t)state)) {
#endif
					return true;
				}

				++count;
			}
			return false;
		}

		inline WaitState * FindWait(uint64_t seq) {
			uint64_t count = 0;
			while (count < MAX_TEST_COUNT) {
#ifdef WIN32
				if (InterlockedCompareExchangeNoFence(&_waits[(seq + count) % MAX_RPC_SZIE], 0, seq) == 0) {
#else
				if (__sync_bool_compare_and_swap(&_waits[(seq + count) % MAX_RPC_SZIE], seq, 0)) {
#endif
					return (WaitState*)seq;
				}

				++count;
			}
			return nullptr;
		}

	private:
		Service _services[MAX_SERVICE_SIZE];
		Service _null;
		std::unordered_map<int32_t, std::function<void(const void * context, int32_t size, RpcRet & ret)>> _cbs;

		uint64_t _waits[MAX_RPC_SZIE];
	};

	Rpc::Rpc() {
		_impl = new RpcImpl;
	}

	Rpc::~Rpc() {
		delete _impl;
	}

	void Rpc::Attach(uint32_t serviceId, int32_t fd, const void * context, int32_t size) {
		_impl->Attach(serviceId, fd, context, size);
	}

	void Rpc::RegisterFn(int32_t rpcId, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn) {
		_impl->RegisterFn(rpcId, fn);
	}

	void Rpc::Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size) {
		_impl->Call(serviceId, rpcId, context, size);
	}

	void Rpc::Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn) {
		_impl->Call(serviceId, rpcId, context, size, fn);
	}
}