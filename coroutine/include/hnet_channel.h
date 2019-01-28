#ifndef __HNET_CHANNEL_H__
#define __HNET_CHANNEL_H__

namespace hyper_net {
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
			const std::function<void(void * src)> recover = CoChannelFunc::Recover;
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
}

#define hn_channel hyper_net::CoChannel
#define hn_channel_close_exception hyper_net::ChannelCloseException

#endif // !__HNET_CHANNEL_H__
