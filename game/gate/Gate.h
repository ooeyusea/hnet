#ifndef __GATE_H__
#define __GATE_H__
#include "hnet.h"
#include "util.h"
#include "Session.h"
#include <map>
#include "spin_mutex.h"

class Gate {
public:
	Gate() {}
	~Gate() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

	inline void RegisterSession(Session * session) {
		std::lock_guard<spin_mutex> lock(_mutex);
		_sessions[session->GetFd()] = session;
	}

	inline void UnregisterSession(Session * session) {
		std::lock_guard<spin_mutex> lock(_mutex);
		_sessions.erase(session->GetFd());
	}

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	spin_mutex _mutex;
	std::map<int32_t, Session*> _sessions;
};

#endif //__GATE_H__
