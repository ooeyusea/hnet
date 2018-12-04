#ifndef __SERVER_NODE_H__
#define __SERVER_NODE_H__
#include "hnet.h"
#include <list>
#include "conf.h"

class Cluster : public Conf {
public:
	static Cluster& Instance() {
		static Cluster g_instance;
		return g_instance;
	}

	bool Start();

	inline int32_t ServiceId(int8_t service, int16_t id) const {
		return service << 16 | id;
	}

	inline void RegisterServiceOpen(const std::function<void(int8_t service, int16_t id)>& f) {
		_openListeners.push_back(f);
	}

	inline void RegisterServiceClose(const std::function<void(int8_t service, int16_t id)>& f) {
		_closeListeners.push_back(f);
	}

	bool ProvideService(int32_t port);
	void RequestSevice(int8_t service, int16_t id, const std::string& ip, int32_t port);

	inline hn_rpc& Get() { return _rpc; }

	inline int16_t GetId() const { return _id; }

	inline void SetType(int8_t type) { _service = type; }

private:
	Cluster();
	~Cluster() {}

	bool _terminate = false;
	int32_t _listenFd = -1;

	hn_rpc _rpc;

	int8_t _service;
	int16_t _id;

	std::list<std::function<void(int8_t service, int16_t id)>> _openListeners;
	std::list<std::function<void(int8_t service, int16_t id)>> _closeListeners;

	int32_t _reconnectInterval;
};

#endif //__SERVER_NODE_H__
