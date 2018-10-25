#ifndef __HNET_H__
#define __HNET_H__
#include <functional>
#include <exception>
#include <type_traits>
#include <memory>

#define HN_IPV4 0
#define HN_IPV6 1

namespace hyper_net {
	struct Forker {
		Forker& operator-(const std::function<void()>& f);
		Forker& operator-(int32_t size);

		int32_t stackSize = 0;
	};

	struct Yielder {
		void Do();
	};

	struct NetAdapter {
		int32_t Connect(const char * ip, const int32_t port, int32_t proto = HN_IPV4);
		int32_t Listen(const char * ip, const int32_t port, int32_t proto = HN_IPV4);
		int32_t Accept(int32_t fd, char * remoteIp = nullptr, int32_t remoteIpSize = 0, int32_t * remotePort = nullptr);
		int32_t Recv(int32_t fd, char * buf, int32_t size);
		void Send(int32_t fd, const char * buf, int32_t size);
		void Close(int32_t fd);
		void Shutdown(int32_t fd);
	};

	struct TimeAdapter {
		int32_t operator-(int64_t millisecond);
		int32_t operator+(int64_t timestamp);
		int32_t operator+(const char * date);
	};

	class CountTickerImpl;
	class CountTicker {
	public:
		class iterator {
		public:
			iterator(CountTicker * ticker) : _ticker(ticker), _end(true) {}
			iterator(CountTicker * ticker, bool end) : _ticker(ticker), _end(end) { }
			~iterator() {}

			inline iterator& operator++() {
				_end = _ticker->Check();
				return *this;
			}

			inline int32_t operator*() const {
				return _ticker->Beat();
			}

			inline bool operator==(const iterator& rhs) const {
				return _end == rhs._end;
			}

			inline bool operator!=(const iterator& rhs) const {
				return _end != rhs._end;
			}

		private:
			bool _end = false;
			CountTicker * _ticker = nullptr;
		};

	public:
		CountTicker(int64_t interval, int32_t count);
		CountTicker(int64_t interval);
		~CountTicker();

		iterator begin() { iterator ret(this, false); return ++ret; }
		iterator& end() { return _last; }

		int32_t Beat();
		bool Check();
		void Stop();
		int32_t GetLastError() const;

	private:
		iterator _last;
		CountTickerImpl * _impl;
	};

	class CoMutexImpl;
	class CoMutex {
	public:
		CoMutex();
		~CoMutex();

		bool try_lock();
		void lock();
		void unlock();
		bool is_locked();

	private:
		CoMutexImpl * _impl;
	};

	class IAsyncQueue {
	public:
		virtual ~IAsyncQueue() {}

		virtual void Call(uint32_t threadId, void * data) = 0;
		virtual void Shutdown() = 0;
	};

	IAsyncQueue * CreateAsyncQueue(int32_t threadCount, bool complete, const std::function<void(void * data)>& f);

	class ChannelCloseException : public std::exception {
	public:
		virtual char const* what() const noexcept { return "channel closed"; }
	};

	class ChannelImpl;
	class Channel {
	public:
		Channel(int32_t blockSize, int32_t capacity);
		~Channel();

		void SetFn(const std::function<void(void * dst, const void * p)>& pushFn, const std::function<void(void * src, void * p)>& popFn, const std::function<void(void * src)>& recoverFn);

		void Push(const void * p);
		void Pop(void * p);

		bool TryPush(const void * p);
		bool TryPop(void * p);

		void Close();

	private:
		ChannelImpl * _impl;
	};
	
	template <typename T, int32_t capacity = -1>
	class CoChannel {
		static_assert(capacity != 0, "channel capacity must not zero");

		struct CoChannelNullFn {
			const std::function<void(void * dst, const void * p)> push = nullptr;
			const std::function<void(void * src, void * p)> pop = nullptr;
			const std::function<void(void * src)> recover = nullptr;
		};

		struct CoChannelFunc {
			static void Push(void * dst, const void * p) {
				new(dst) T(*(T*)p);
			}

			static void Pop(void * src, void * p) {
				*(T*)p = std::move(*(T*)src);
				(*(T*)src).~T();
			}

			static void Recover(void * src) {
				(*(T*)src).~T();
			}

			const std::function<void(void * dst, const void * p)> push = CoChannelFunc::Push;
			const std::function<void(void * src, void * p)> pop = CoChannelFunc::Pop;
			const std::function<void(void * src, void * p)> recover = CoChannelFunc::Recover;
		};

		typedef typename std::conditional<std::is_pod<T>::value, CoChannelNullFn, CoChannelFunc>::type FuncType;
	public:
		CoChannel() {
			_impl.reset(new Channel(sizeof(T), capacity));
			_impl->SetFn(_type.push, _type.pop, _type.recover);
		}
		~CoChannel() {}

		inline  const CoChannel& operator<<(const T& t) const {
			_impl->Push(&t);
			return *this;
		}

		inline  const CoChannel& operator>>(T& t) const {
			_impl->Pop(&t);
			return *this;
		}

		inline bool TryPush(const T& t) const {
			return _impl->TryPush(&t);
		}

		inline bool TryPop(T& t) const {
			return _impl->TryPop(&t);
		}

		inline void Close() const {
			_impl->Close();
		}

	private:
		FuncType _type;
		mutable std::shared_ptr<Channel> _impl;
	};

//	class RpcException : public std::exception {
//	public:
//		virtual char const* what() const noexcept { return "rpc failed"; }
//	};
//
//	struct IRpcEncoder {
//		virtual ~IRpcEncoder() {}
//
//		virtual int32_t CalcEncode(int32_t rpcId, void * p) = 0;
//		virtual bool Encode(int32_t rpcId, void * p, void * dst, int32_t size) = 0;
//		virtual bool Decode(int32_t rpcId, void * p, const void * src, int32_t size) = 0;
//	};
//
//	class CoRpcImpl;
//	class CoRpc {
//	public:
//		CoRpc();
//		~CoRpc();
//
//		bool DialRpc(const char * ip, const int32_t port);
//		bool HandleRpc(const char * ip, const int32_t port);
//
//		template <typename P, typename R>
//		inline R CallR(int32_t rpcId, const P & p) {
//			if (!_encoder) {
//				R r;
//				Call(rpcId, &p, sizeof(p), &r, sizeof(r));
//				return r;
//			}
//			else {
//				int32_t size = _encoder->CalcEncode(rpcId, p);
//				char data[size];
//				if (_encoder->Encode(rpcId, p, data, size)) {
//					Call(rpcId, data, size, &r, sizeof(r));
//				}
//				else
//					throw RpcException();
//			}
//		}
//
//		template <typename P>
//		inline void Call(int32_t rpcId, const P & p) {
//			Call(rpcId, &p, sizeof(p));
//		}
//
//		void SetEncoder(IRpcEncoder * encoder) { _encoder = encoder; }
//
//	protected:
//		void Call(int32_t rpcId, const void * context, int32_t size, void * resContext, int32_t resSize);
//		void Call(int32_t rpcId, const void * context, int32_t size);
//
//	private:
//		CoRpcImpl * _impl;
//		IRpcEncoder * _encoder = nullptr;
//	};
}

#define hn_fork hyper_net::Forker()-
#define hn_stack(size) (size)-
#define hn_yield hyper_net::Yielder().Do()

#define hn_connect_p(ip, port, proto) hyper_net::NetAdapter().Connect(ip, port, proto)
#define hn_listen_p(ip, port, proto) hyper_net::NetAdapter().Listen(ip, port, proto)
#define hn_connect(ip, port) hyper_net::NetAdapter().Connect(ip, port)
#define hn_listen(ip, port) hyper_net::NetAdapter().Listen(ip, port)
#define hn_accept(fd) hyper_net::NetAdapter().Accept(fd)
#define hn_accept_addr(fd, ip, size, port) hyper_net::NetAdapter().Accept(fd, ip, size, port)
#define hn_recv(fd, buf, size) hyper_net::NetAdapter().Recv(fd, buf, size)
#define hn_send(fd, buf, size) hyper_net::NetAdapter().Send(fd, buf, size)
#define hn_close(fd) hyper_net::NetAdapter().Close(fd)
#define hn_shutdown(fd) hyper_net::NetAdapter().Close(fd)

#define hn_sleep hyper_net::TimeAdapter()-
#define hn_wait hyper_net::TimeAdapter()+
#define hn_ticker hyper_net::CountTicker

#define hn_mutex hyper_net::CoMutex

#define hn_create_async hyper_net::CreateAsyncQueue

#define hn_channel(T, capicity) hyper_net::CoChannel<T, capicity>
#define hn_channel_close_exception hyper_net::ChannelCloseException

#endif // !__HNET_H__
