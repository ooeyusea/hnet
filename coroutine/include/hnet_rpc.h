#ifndef __HNET_RPC_H__
#define __HNET_RPC_H__

namespace hyper_net {
	const uint32_t JUST_CALL = 0;

	class RpcException : public std::exception {
	public:
		virtual char const* what() const noexcept { return "rpc failed"; }
	};

	class RpcRetImpl;
	class RpcImpl;

	class RpcRet {
		friend class RpcImpl;
	public:
		RpcRet();
		~RpcRet();

		void Ret(const void * context, int32_t size);
		void Fail();

	private:
		RpcRetImpl * _impl;
	};

	class Rpc {
	public:
		Rpc();
		~Rpc();

		void Attach(uint32_t serviceId, int32_t fd);
		void Start(uint32_t serviceId, int32_t fd, const char * context, int32_t size);
		void RegisterFn(int32_t rpcId, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn);
		void RegisterFn(int32_t rpcId, const std::function<int64_t(const void * context, int32_t size)>& order, 
			const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn);
		void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size);
		void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn);

	private:
		RpcImpl * _impl;
	};

	template <int32_t size, typename R>
	struct Callback {
		template <typename... ParsedArgs>
		struct Dealer {
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, R(*fn)(ParsedArgs...), ParsedArgs... args) {
				R r = fn(args...);

				char buff[size];
				OBufferStream stream(buff, size);
				OArchiver<OBufferStream> ar(stream, 0);
				ar << r;
				if (stream.bad())
					ret.Fail();
				else
					ret.Ret(buff, (int32_t)stream.size());
			}

			template <typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, R(*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, fn, args..., t);
			}

			template <typename C>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, R(C::*fn)(ParsedArgs...), ParsedArgs... args) {
				R r = (c.*fn)(args...);

				char buff[size];
				OBufferStream stream(buff, size);
				OArchiver<OBufferStream> ar(stream, 0);
				ar << r;
				if (stream.bad())
					ret.Fail();
				else
					ret.Ret(buff, (int32_t)stream.size());
			}

			template <typename C, typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, R(C::*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}

			template <typename C>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, R(C::*fn)(ParsedArgs...) const, ParsedArgs... args) {
				R r = (c.*fn)(args...);

				char buff[size];
				OBufferStream stream(buff, size);
				OArchiver<OBufferStream> ar(stream, 0);
				ar << r;
				if (stream.bad())
					ret.Fail();
				else
					ret.Ret(buff, (int32_t)stream.size());
			}

			template <typename C, typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, R(C::*fn)(ParsedArgs..., T, Args...) const, ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}
		};

		template <typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, R(*fn)(Args...)) {
			Dealer<>::invoke(reader, ret, fn);
		}

		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, R(*fn)()) {
			R r = fn();

			char buff[size];
			OBufferStream stream(buff, size);
			OArchiver<OBufferStream> ar;
			ar << r;
			if (stream.bad())
				ret.Fail();
			else
				ret.Ret(buff, (int32_t)stream.size());
		}

		template <typename C, typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, R(C::*fn)(Args...)) {
			Dealer<>::invoke(reader, ret, c, fn);
		}

		template <typename C>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, R(C::*fn)()) {
			R r = (c.*fn)();

			char buff[size];
			OBufferStream stream(buff, size);
			OArchiver<OBufferStream> ar(stream, 0);
			ar << r;
			if (stream.bad())
				ret.Fail();
			else
				ret.Ret(buff, (int32_t)stream.size());
		}

		template <typename C, typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, R(C::*fn)(Args...) const) {
			Dealer<>::invoke(reader, ret, c, fn);
		}

		template <typename C>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, R(C::*fn)() const) {
			R r = (c.*fn)();

			char buff[size];
			OBufferStream stream(buff, size);
			OArchiver<OBufferStream> ar(stream, 0);
			ar << r;
			if (stream.bad())
				ret.Fail();
			else
				ret.Ret(buff, (int32_t)stream.size());
		}
	};

	template <int32_t size>
	struct Callback<size, void> {
		template <typename... ParsedArgs>
		struct Dealer {
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, void(*fn)(ParsedArgs...), ParsedArgs... args) {
				fn(args...);
			}

			template <typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, void(*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, fn, args..., t);
			}

			template <typename C>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)(ParsedArgs...), ParsedArgs... args) {
				(c.*fn)(args...);
			}

			template <typename C, typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}

			template <typename C>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, void (C::*fn)(ParsedArgs...) const, ParsedArgs... args) {
				(c.*fn)(args...);
			}

			template <typename C, typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, void (C::*fn)(ParsedArgs..., T, Args...) const, ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}
		};

		template <typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, void(*fn)(Args...)) {
			Dealer<>::invoke(reader, ret, fn);
		}

		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, void(*fn)()) {
			fn();
		}

		template <typename C, typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)(Args...)) {
			Dealer<>::invoke(reader, ret, c, fn);
		}

		template <typename C>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)()) {
			(c.*fn)();
		}

		template <typename C, typename... Args>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, void (C::*fn)(Args...) const) {
			Dealer<>::invoke(reader, ret, c, fn);
		}

		template <typename C>
		static void Deal(IArchiver<IBufferStream> & reader, RpcRet & ret, const C & c, void (C::*fn)() const) {
			(c.*fn)();
		}
	};

	struct Order {
		template <typename... ParsedArgs>
		struct Dealer {
			static int64_t invoke(IArchiver<IBufferStream> & reader, int64_t (*fn)(ParsedArgs...), ParsedArgs... args) {
				return fn(args...);
			}

			template <typename T, typename... Args>
			static int64_t invoke(IArchiver<IBufferStream> & reader, int64_t (*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
				reader >> t;
				if (reader.Fail())
					return 0;

				return Dealer<ParsedArgs..., T>::invoke(reader, fn, args..., t);
			}

			template <typename C>
			static int64_t invoke(IArchiver<IBufferStream> & reader, C & c, int64_t(C::*fn)(ParsedArgs...), ParsedArgs... args) {
				return (c.*fn)(args...);
			}

			template <typename C, typename T, typename... Args>
			static int64_t invoke(IArchiver<IBufferStream> & reader, C & c, int64_t(C::*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail()) 
					return 0;

				return Dealer<ParsedArgs..., T>::invoke(reader, c, fn, args..., t);
			}

			template <typename C>
			static int64_t invoke(IArchiver<IBufferStream> & reader, const C & c, int64_t(C::*fn)(ParsedArgs...) const, ParsedArgs... args) {
				return (c.*fn)(args...);
			}

			template <typename C, typename T, typename... Args>
			static int64_t invoke(IArchiver<IBufferStream> & reader, const C & c, int64_t(C::*fn)(ParsedArgs..., T, Args...) const, ParsedArgs... args) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail())
					return 0;

				return Dealer<ParsedArgs..., T>::invoke(reader, c, fn, args..., t);
			}
		};

		template <typename... Args>
		static int64_t Deal(IArchiver<IBufferStream> & reader, int64_t(*fn)(Args...)) {
			return Dealer<>::invoke(reader, fn);
		}

		static int64_t Deal(IArchiver<IBufferStream> & reader, int64_t(*fn)()) {
			return fn();
		}

		template <typename C, typename... Args>
		static int64_t Deal(IArchiver<IBufferStream> & reader, C & c, int64_t(C::*fn)(Args...)) {
			return Dealer<>::invoke(reader, c, fn);
		}

		template <typename C>
		static int64_t Deal(IArchiver<IBufferStream> & reader, C & c, int64_t(C::*fn)()) {
			return (c.*fn)();
		}

		template <typename C, typename... Args>
		static int64_t Deal(IArchiver<IBufferStream> & reader, const C & c, int64_t(C::*fn)(Args...) const) {
			return Dealer<>::invoke(reader, c, fn);
		}

		template <typename C>
		static int64_t Deal(IArchiver<IBufferStream> & reader, const C & c, int64_t(C::*fn)() const) {
			return (c.*fn)();
		}
	};

	struct FunctionTrait {};
	struct LamdaTrait {};

	class CoRpc {
	public:
		class RpcRegister {
			friend class CoRpc;
		public:
			~RpcRegister() {}

			template <typename F>
			inline RpcRegister& AddOrder(const F& fn) {
				typedef typename std::conditional<std::is_function<F>::value, FunctionTrait, LamdaTrait>::type TraitType;
				AddOrderTrait(fn, TraitType());
				return *this;
			}

			template <typename C, typename... Args>
			inline RpcRegister& AddOrder(const C & c, int64_t (C::* fn)(Args...) const) {
				_order = [c, fn, this](const void * context, int32_t size) -> int64_t {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);

					return Order::Deal<C, Args...>(reader, c, fn);
				};
				return *this;
			}

			template <typename C, typename... Args>
			inline RpcRegister& AddOrder(C & c, int64_t (C::* fn)(Args...)) {
				_order = [&c, fn, this](const void * context, int32_t size) -> int64_t {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);
					return Order::Deal(reader, c, fn);
				};
				return *this;
			}

			template <int32_t buffSize, typename F>
			inline RpcRegister& AddCallback(const F& fn) {
				typedef typename std::conditional<std::is_function<F>::value, FunctionTrait, LamdaTrait>::type TraitType;
				AddCallbackTrait<buffSize>(fn, TraitType());
				return *this;
			}

			template <int32_t buffSize, typename C, typename R, typename... Args>
			inline RpcRegister& AddCallback(const C & c, R(C::* fn)(Args...) const) {
				_callback = [c, fn, this](const void * context, int32_t size, RpcRet & ret) {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);

					Callback<buffSize, R>::Deal<C, Args...>(reader, ret, c, fn);
				};
				return *this;
			}

			template <int32_t buffSize, typename C, typename R, typename... Args>
			inline RpcRegister& AddCallback(C & c, R(C::* fn)(Args...)) {
				_callback = [&c, fn, this](const void * context, int32_t size, RpcRet & ret) {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);
					Callback<buffSize, R>::Deal(reader, ret, c, fn);
				};
				return *this;
			}

			inline void Comit() {
				_impl.RegisterFn(_rpcId, _order, _callback);
			}

		private:
			RpcRegister(Rpc& impl, int32_t rpcId) : _impl(impl), _rpcId(rpcId) {}

			template <int32_t buffSize, typename R, typename... Args>
			inline void AddCallbackTrait(R(*fn)(Args...), const FunctionTrait &) {
				_callback = [fn, this](const void * context, int32_t size, RpcRet & ret) {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);
					Callback<buffSize, R>::Deal(reader, ret, fn);
				};
			}

			template <int32_t buffSize, typename F>
			inline void AddCallbackTrait(const F& fn, const LamdaTrait &) {
				AddCallback<buffSize, std::remove_reference_t<std::remove_const_t<F>>>(fn, &F::operator());
			}

			template <typename... Args>
			inline void AddOrderTrait(int64_t (*fn)(Args...), const FunctionTrait &) {
				_order = [fn, this](const void * context, int32_t size) -> int64_t {
					IBufferStream istream((const char*)context, size);
					IArchiver<IBufferStream> reader(istream, 0);
					return Order::Deal(reader, fn);
				};
			}

			template <typename F>
			inline void AddOrderTrait(const F& fn, const LamdaTrait &) {
				AddOrder<std::remove_reference_t<std::remove_const_t<F>>>(fn, &F::operator());
			}

		private:
			Rpc& _impl;
			int32_t _rpcId = 0;
			std::function<int64_t(const void * context, int32_t size)> _order;
			std::function<void(const void * context, int32_t size, RpcRet & ret)> _callback;
		};

		class Callor {
			friend class CoRpc;
		public:
			~Callor() {}

			inline void Push(OArchiver<OBufferStream> & ar) {

			}

			template <typename T, typename... Args>
			inline void Push(OArchiver<OBufferStream> & ar, T& t, Args... args) {
				ar << t;
				if (ar.Fail())
					return;

				Push(ar, args...);
			}

			template <typename R, int32_t size, typename... Args>
			inline R Do(int32_t rpcId, Args... args) {
				char buff[size];
				OBufferStream stream(buff, size);
				OArchiver<OBufferStream> ar(stream, 0);
				Push(ar, args...);

				R r;
				_impl.Call(_serviceId, rpcId, buff, (int32_t)stream.size(), [&r, this](const void * context, int32_t len) -> bool {
					IBufferStream istream((const char*)context, len);
					IArchiver<IBufferStream> reader(istream, 0);
					reader >> r;
					return !reader.Fail();
				});

				return r;
			}

			template <int32_t size, typename... Args>
			inline void Do(int32_t rpcId, Args... args) {
				char buff[size];
				OBufferStream stream(buff, size);
				OArchiver<OBufferStream> ar(stream, 0);
				Push(ar, args...);

				_impl.Call(_serviceId, rpcId, buff, (int32_t)stream.size());
			}

		private:
			Callor(Rpc& impl, int32_t serviceId) : _impl(impl), _serviceId(serviceId) {}

		private:
			Rpc& _impl;
			int32_t _serviceId = 0;
		};

	public:
		CoRpc() {}
		~CoRpc() {}

		inline void Attach(uint32_t serviceId, int32_t fd) {
			_impl.Attach(serviceId, fd);
		}

		inline void Start(uint32_t serviceId, int32_t fd, const char * context = nullptr, int32_t size = 0) {
			_impl.Start(serviceId, fd, context, size);
		}

		inline RpcRegister Register(int32_t rpcId) {
			return RpcRegister(_impl, rpcId);
		}

		inline Callor Call(int32_t serviceId) {
			return Callor(_impl, serviceId);
		}

	private:
		Rpc _impl;
	};
}

#define hn_rpc hyper_net::CoRpc
#define hn_rpc_exception hyper_net::RpcException

#endif // !__HNET_RPC_H__
