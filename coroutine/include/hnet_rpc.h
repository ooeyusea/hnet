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
		void Call(uint32_t serviceId, int32_t rpcId, const void * context, int32_t size, const std::function<bool (const void * data, int32_t size)>& fn);

	private:
		RpcImpl * _impl;
	};

	template <typename Decoder>
	class CoRpc {
	public:
		CoRpc() {}
		~CoRpc() {}

		inline void Attach(uint32_t serviceId, int32_t fd, const void * context = nullptr, int32_t size = 0) {
			_impl.Attach(serviceId, fd, context, size);
		}

		template <typename P>
		inline void RegisterFn(int32_t rpcId, const std::function<void(const P& p)>& fn) {
			_impl.RegisterFn(rpcId, [fn, this](const void * context, int32_t size, RpcRet & ret){
				P p;
				if (_decoder.Decode(p, context, size)) {
					fn(p);
				}
				else
					throw RpcException();
			});
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
