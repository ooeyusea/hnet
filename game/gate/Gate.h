#ifndef __GATE_H__
#define __GATE_H__
#include "hnet.h"
#include "util.h"
#include "Session.h"
#include <map>
#include "spin_mutex.h"
#include "conf.h"

class Gate : public Conf {
public:
	Gate();
	~Gate() {}

	bool Start();
	void Run();
	void Release();
	inline void Terminate() { _closeCh.TryPush(1); }

	inline void RegisterSession(Session * session) {
		std::lock_guard<spin_mutex> lock(_mutex);
		_sessions[session->GetFd()] = session;
	}

	inline void UnregisterSession(Session * session) {
		std::lock_guard<spin_mutex> lock(_mutex);
		_sessions.erase(session->GetFd());
	}

private:
	bool ReadConf();

private:
	int32_t _listenPort = 0;
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	int16_t _gateId;

	spin_mutex _mutex;
	std::map<int32_t, Session*> _sessions;

	int32_t _delay = 0;
};

#endif //__GATE_H__
