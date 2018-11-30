#ifndef __EVENTMGR_H__
#define __EVENTMGR_H__
#include "util.h"
#include <list>
#include <unordered_map>

class EventMgr {
	struct Callback {
		std::function<void(const char *, int32_t)> fn;
		std::string file;
		int32_t line;
	};

	struct JudgeCb {
		std::function<bool(const char *, int32_t)> fn;
		std::string file;
		int32_t line;
	};

public:
	EventMgr() {}
	~EventMgr() {}

	template <typename T>
	inline void RegEvent(int32_t eventId, const char * file, int32_t line, const std::function<void(T&)>& fn) {
		_cbs[eventId].push_back({ [fn](const char * context, int32_t size) {
			if (size == sizeof(T))
				fn(*(T*)context);
		}, file, line });
	}

	template <typename T>
	inline void RegJudge(int32_t eventId, const char * file, int32_t line, const std::function<bool(T&)>& fn) {
		_cbs[eventId].push_back({ [fn](const char * context, int32_t size) {
			if (size == sizeof(T))
				return fn(*(T*)context);
			return false;
		}, file, line });
	}

	template <typename T>
	inline void Do(int32_t eventId, T& t) {
		auto itr = _cbs.find(eventId);
		if (itr != _cbs.end()) {
			for (auto& cb : itr->second) {
				cb.fn((const char*)&t, sizeof(T));
			}
		}
	}

	template <typename T>
	inline bool Judge(int32_t eventId, T& t) {
		auto itr = _judges.find(eventId);
		if (itr != _judges.end()) {
			for (auto& cb : itr->second) {
				if (!cb.fn((const char*)&t, sizeof(T)))
					return false;
			}
		}
		return true;
	}

private:
	std::unordered_map<int32_t, std::list<Callback>> _cbs;
	std::unordered_map<int32_t, std::list<JudgeCb>> _judges;
};

extern EventMgr g_eventMgr;

#endif //__EVENTMGR_H__