#ifndef __HNET_RPC_H__
#define __HNET_RPC_H__

namespace hyper_net {
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

		void Attach(uint32_t serviceId, int32_t fd, const void * context, int32_t size);
		void RegisterFn(int32_t rpcId, const std::function<void(const void * context, int32_t size, RpcRet & ret)>& fn);
		void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size);
		void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool(const void * data, int32_t size)>& fn);

	private:
		RpcImpl * _impl;
	};

	template <int32_t size, typename R>
	struct FHandler {
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
	};

	template <int32_t size, typename R>
	struct CHandler {
		template <typename... ParsedArgs>
		struct Dealer {
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
				T t;
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
				T t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}
		};

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
	struct CHandler<size, void> {
		template <typename... ParsedArgs>
		struct Dealer {
			template <typename C>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)(ParsedArgs...), ParsedArgs... args) {
				(c.*fn)(args...);
			}

			template <typename C, typename T, typename... Args>
			static void invoke(IArchiver<IBufferStream> & reader, RpcRet & ret, C & c, void (C::*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
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
				T t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				Dealer<ParsedArgs..., T>::invoke(reader, ret, c, fn, args..., t);
			}
		};

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

	struct FunctionTrait {};
	struct LamdaTrait {};

	class CoRpc {
	public:
		CoRpc() {}
		~CoRpc() {}

		inline void Attach(uint32_t serviceId, int32_t fd, const void * context = nullptr, int32_t size = 0) {
			_impl.Attach(serviceId, fd, context, size);
		}

		template <int32_t buffSize, typename F>
		inline void RegisterFn(int32_t rpcId, const F& fn) {
			typedef typename std::conditional<std::is_function<F>::value, FunctionTrait, LamdaTrait>::type TraitType;
			RegisterFnTrait<buffSize>(rpcId, fn, TraitType());
		}

		template <int32_t buffSize, typename C, typename R, typename... Args>
		inline void RegisterFn(int32_t rpcId, const C & c, R(C::* fn)(Args...) const) {
			_impl.RegisterFn(rpcId, [c, fn, this](const void * context, int32_t size, RpcRet & ret) {
				IBufferStream istream((const char*)context, size);
				IArchiver<IBufferStream> reader(istream, 0);

				CHandler<buffSize, R>::Deal<C, Args...>(reader, ret, c, fn);
			});
		}

		template <int32_t buffSize, typename C, typename R, typename... Args>
		inline void RegisterFn(int32_t rpcId, C & c, R(C::* fn)(Args...)) {
			_impl.RegisterFn(rpcId, [&c, fn, this](const void * context, int32_t size, RpcRet & ret) {
				IBufferStream istream((const char*)context, size);
				IArchiver<IBufferStream> reader(istream, 0);
				CHandler<buffSize, R>::Deal(reader, ret, c, fn);
			});
		}

		template <int32_t buffSize, typename R, typename... Args>
		inline void RegisterFnTrait(int32_t rpcId,  R (* fn)(Args...), const FunctionTrait &) {
			_impl.RegisterFn(rpcId, [fn, this](const void * context, int32_t size, RpcRet & ret) {
				IBufferStream istream((const char*)context, size);
				IArchiver<IBufferStream> reader(istream, 0);
				FHandler<buffSize, R>::Deal(reader, ret, fn);
			});
		}

		template <int32_t buffSize, typename F>
		inline void RegisterFnTrait(int32_t rpcId, const F& fn, const LamdaTrait &) {
			RegisterFn<buffSize, std::remove_reference_t<std::remove_const_t<F>>>(rpcId, fn, &F::operator());
		}

		inline void Push(OArchiver<OBufferStream> & ar) {

		}

		template <typename T, typename... Args>
		inline void Push(OArchiver<OBufferStream> & ar, const T& t, Args... args) {
			ar << t;
			if (ar.Fail())
				return;

			Push(ar, args...);
		}

		template <typename R, int32_t size, typename... Args>
		inline R Call(int32_t serviceId, int32_t rpcId, Args... args) {
			char buff[size];
			OBufferStream stream(buff, size);
			OArchiver<OBufferStream> ar(stream, 0);
			Push(ar, args...);

			R r;
			_impl.Call(serviceId, rpcId, buff, (int32_t)stream.size(), [&r, this](const void * context, int32_t size) -> bool {
				IBufferStream istream((const char*)context, size);
				IArchiver<IBufferStream> reader(istream, 0);
				reader >> r;
				return !reader.Fail();
			});

			return r;
		}

		template <int32_t size, typename... Args>
		inline void Call(int32_t serviceId, int32_t rpcId, Args... args) {
			char buff[size];
			OBufferStream stream(buff, size);
			OArchiver<OBufferStream> ar(stream, 0);
			Push(ar, args...);

			_impl.Call(serviceId, rpcId, buff, (int32_t)stream.size());
		}

	private:
		Rpc _impl;
	};
}

#define hn_rpc hyper_net::CoRpc
#define hn_rpc_exception hyper_net::RpcException

#endif // !__HNET_RPC_H__
