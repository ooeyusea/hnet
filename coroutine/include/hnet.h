#ifndef __HNET_H__
#define __HNET_H__
#include <functional>

#define DEFAULT_STACK_SIZE 64 * 1024
#define HN_IPV4 0
#define HN_IPV6 1

namespace hyper_net {
	struct Forker {
		Forker& operator-(const std::function<void()>& f);
		Forker& operator-(int32_t size);

		int32_t stackSize = DEFAULT_STACK_SIZE;
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

	class ChannelImpl;
	class Channel {
	public:
		Channel(int32_t blockSize, int32_t capacity);
		~Channel();

		void Push(const void * p);
		void Pop(void * p);

		bool TryPush(const void * p);
		bool TryPop(void * p);

	private:
		ChannelImpl * _impl;
	};
	
	template <typename T, int32_t capacity = -1>
	class CoChannel {
		static_assert(capacity != 0, "channel capacity must not zero");
	public:
		CoChannel() : _impl(sizeof(T), capacity) {}
		~CoChannel() {}

		inline CoChannel& operator<<(const T& t) {
			_impl.Push(&t);
			return *this;
		}

		inline CoChannel& operator>>(T& t) {
			_impl.Pop(&t);
			return *this;
		}

		inline bool TryPush(const T& t) {
			return _impl.TryPush(&t);
		}

		inline bool TryPop(T& t) {
			return _impl.TryPop(&t);
		}

	private:
		Channel _impl;
	};
}

#define hn_fork hyper_net::Forker()-
#define hn_stack(size) (size)-
#define hn_yield hyper_net::Yielder().Do()

#define hn_connect(ip, port) hyper_net::NetAdapter().Connect(ip, port)
#define hn_listen(ip, port) hyper_net::NetAdapter().Listen(ip, port)
#define hn_accept(fd) hyper_net::NetAdapter().Accept(fd)
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

#endif // !__HNET_H__
