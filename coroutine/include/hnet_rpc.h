#ifndef __HNET_RPC_H__
#define __HNET_RPC_H__
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <vector>
#include <string>

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

	template <typename R, typename Reader, typename OStream, typename OArchiver, int32_t size>
	struct Handler {
		template <typename... ParsedArgs>
		struct Dealer {
			static void invoke(Reader & reader, RpcRet & ret, R(*fn)(ParsedArgs...), ParsedArgs... args) {
				R r = fn(args...);

#ifndef WIN32
				char buff[size];
#else
				char * buff = (char*)alloc(size);
#endif
				OStream stream(buff, size);
				OArchiver<OStream> ar;
				ar << r;
				if (stream.bad())
					ret.Fail();
				else
					ret.Ret(buff, stream.size());
			}

			template <typename T, typename... Args>
			static void invoke(Reader & reader, RpcRet & ret, R(*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
				reader >> t;
				if (reader.Fail()) {
					ret.Fail();
					return;
				}

				return Dealer<ParsedArgs..., T>::invoke(reader, ret, fn, args..., t);
			}

			template <typename C>
			static void invoke(Reader & reader, RpcRet & ret, C c, R(C::*fn)(ParsedArgs...), ParsedArgs... args) {
				R r = (c.*fn)(args...);

#ifndef WIN32
				char buff[size];
#else
				char * buff = (char*)alloc(size);
#endif
				OStream stream(buff, size);
				OArchiver<OStream> ar;
				ar << r;
				if (stream.bad())
					ret.Fail();
				else
					ret.Ret(buff, stream.size());
			}

			template <typename C, typename T, typename... Args>
			static R invoke(Reader & reader, RpcRet & ret, C c, R(C::*fn)(ParsedArgs..., T, Args...), ParsedArgs... args) {
				T t;
				reader >> t;
				if (reader.Fail())
					throw RpcException();

				return Dealer<ParsedArgs..., T>::invoke(reader, c, fn, args..., t);
			}
		};

		template <typename... Args>
		static R Deal(Reader & reader, R(*fn)(Args...)) {
			return Dealer<>::invoke(reader, fn);
		}

		static R Deal(Reader & reader, R(*fn)()) {
			return fn();
		}

		template <typename C, typename... Args>
		static R Deal(Reader & reader, C c, R(C::*fn)(Args...)) {
			return Dealer<>::invoke(reader, c, fn);
		}

		template <typename C>
		static R Deal(Reader & reader, C c, R(C::*fn)()) {
			return (c.*fn)fn();
		}
	};

	struct FunctionTrait {};
	struct LamdaTrait {};

	template <typename Decoder>
	class CoRpc {
	public:
		CoRpc() {}
		~CoRpc() {}

		inline void Attach(uint32_t serviceId, int32_t fd, const void * context = nullptr, int32_t size = 0) {
			_impl.Attach(serviceId, fd, context, size);
		}

		template <typename F>
		inline void RegisterFn(int32_t rpcId, const F& fn) {
			typedef typename std::conditional<std::is_function<F>::value, FunctionTrait, LamdaTrait>::type TraitType;
			RegisterFn(rpcId, fn, TraitType());
		}

		template <typename C, typename R, typename... Args>
		inline void RegisterFn(int32_t rpcId, const C& c, R(C::* fn)(Args...)) {
			_impl.RegisterFn(rpcId, [c, fn, this](const void * context, int32_t size, RpcRet & ret) {

			});
		}

		template <typename C, typename R, typename... Args>
		inline void RegisterFn(int32_t rpcId, C& c, R(C::* fn)(Args...)) {
			_impl.RegisterFn(rpcId, [&c, fn, this](const void * context, int32_t size, RpcRet & ret) {

			});
		}

		template <typename R, typename... Args>
		inline void RegisterFn(int32_t rpcId,  R (* fn)(Args...), const FunctionTrait &) {
			_impl.RegisterFn(rpcId, [fn, this](const void * context, int32_t size, RpcRet & ret) {
				
			});
		}

		template <typename F>
		inline void RegisterFn(int32_t rpcId, const F& fn, const LamdaTrait &) {
			RegisterFn(fpcId, fn, F::operator());
		}

		template <typename P, typename R>
		inline void RegisterFn(int32_t rpcId, const std::function<R (const P& p)>& fn) {
			_impl.RegisterFn(rpcId, [fn, this](const void * context, int32_t size, RpcRet & ret) {
				P p;
				if (_decoder.Decode(p, context, size)) {
					R r = fn(p);
					int32_t size = _decoder.CalcEncode(r);
#ifndef WIN32
					char data[size];
#else
					char * data = (char*)alloca(size);
#endif
					if (_decoder.Encode(r, data, size)) {
						ret.Ret(data, size);
					}
					else
						throw RpcException();
				}
				else
					throw RpcException();
			});
		}

		template <typename P, typename R>
		inline R CallR(uint32_t serviceId, int32_t rpcId, const P & p) {
			int32_t size = _decoder.CalcEncode(p);
#ifndef WIN32
			char data[size];
#else
			char * data = (char*)alloca(size);
#endif
			if (_decoder.Encode(p, data, size)) {
				R r;
				_impl.Call(serviceId, rpcId, data, size, [&r, this](const void * context, int32_t size) -> bool {
					return _decoder.Decode(r, context, size);
				});

				return r;
			}
			else
				throw RpcException();
		}

		template <typename P>
		inline void Call(uint32_t serviceId, int32_t rpcId, const P & p) {
			int32_t size = _decoder.CalcEncode(rpcId, p);
#ifndef WIN32
			char data[size];
#else
			char * data = (char*)alloca(size);
#endif
			if (_encoder.Encode(p, data, size)) {
				_impl.Call(serviceId, rpcId, data, size);
			}
			else
				throw RpcException();
		}

	private:
		Rpc _impl;
		Decoder _decoder;
	};
}

#define hn_rpc hyper_net::CoRpc
#define hn_rpc_exception hyper_net::RpcException

#endif // !__HNET_RPC_H__
