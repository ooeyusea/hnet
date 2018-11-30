#ifndef __METHOD_H__
#define __METHOD_H__
#include "hnet.h"
#include "util.h"
#include <unordered_map>

namespace method {
	struct FunctionTrait {};
	struct LamdaTrait {};

	template <typename MsgType, typename... Args>
	class MethodCallor {
	private:
		struct Method {
			template <typename T>
			inline static bool Deal(hn_iachiver & reader, Args... args, void(*fn)(Args..., T)) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail())
					return false;

				fn(args..., t);
				return true;
			}

			template <typename C, typename T>
			inline static bool Deal(hn_iachiver & reader, Args... args, C & c, void(C::*fn)(Args..., T)) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail())
					return false;

				(c.*fn)(args..., t);
				return true;
			}


			template <typename C, typename T>
			inline static bool Deal(hn_iachiver & reader, Args... args, const C & c, void(C::*fn)(Args..., T) const) {
				std::remove_const_t<std::remove_reference_t<T>> t;
				reader >> t;
				if (reader.Fail())
					return false;

				(c.*fn)(args..., t);
				return true;
			}
		};

		struct Callback {
			std::function<bool (hn_iachiver & ar, Args... args)> fn;
		};

	public:
		MethodCallor() {}
		virtual ~MethodCallor() {}

		template <typename F>
		inline void Register(const MsgType & type, const F& fn) {
			typedef typename std::conditional<std::is_function<F>::value, FunctionTrait, LamdaTrait>::type TraitType;
			RegisterTrait(type, fn, TraitType());
		}

		template <typename C, typename T>
		inline void Register(const MsgType & type, const C & c, void (C::* fn)(Args..., T) const) {
			_methods[type].fn = [c, fn](hn_iachiver & ar, Args... args) -> bool {
				return Method::Deal(ar, args..., c, fn);
			};
		}

		template <typename C, typename T>
		inline void Register(const MsgType & type, C & c, void (C::* fn)(Args..., T)) {
			_methods[type].fn = [&c, fn](hn_iachiver & ar, Args... args) -> bool {
				return Method::Deal(ar, args..., c, fn);
			};
		}

	protected:
		inline bool Deal(const MsgType & type, hn_iachiver & ar, Args... args) {
			auto itr = _methods.find(type);
			if (itr != _methods.end())
				return itr->second.fn(ar, args...);
			return false;
		}
		
	private:
		template <typename T>
		inline void RegisterTrait(const MsgType & type, void (*fn)(Args..., T), const FunctionTrait &) {
			_methods[type].fn = [fn](hn_iachiver & ar, Args... args) -> bool {
				return Method::Deal(ar, args..., fn);
			};
		}

		template <typename F>
		inline void RegisterTrait(const MsgType & type, const F& fn, const LamdaTrait &) {
			Register<std::remove_reference_t<std::remove_const_t<F>>>(type, fn, &F::operator());
		}

	private:
		std::unordered_map<MsgType, Callback> _methods;
	};
};

#endif // !__METHOD_H__
