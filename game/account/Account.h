#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__
#include "hnet.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "nodedefine.h"
#include "conf.h"
#include <vector>

class Account : public Conf {
	struct User {
		int16_t gateId = 0;
		int8_t kickCount = 0;
		int64_t check = 0;
		int32_t fd = -1;
	};
	
	struct Gate {
		int16_t id = 0;
		int64_t activeTime = 0;
		int32_t count = 0;

		std::string ip;
		int16_t port = 0;
	};

	typedef LockTable<std::string, User, spin_mutex, fake_mutex> UserTable;

public:
	Account() {
		_gates.resize(MAX_ZONE);
		for (auto& v : _gates)
			v.resize(MAX_ZONE_ID);
	}
	~Account() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

	Gate * Choose(int16_t zone);

private:
	int32_t _listenFd;
	hn_channel<int8_t, 1> _closeCh;

	UserTable _users;

	std::vector<std::vector<Gate>> _gates;

	int32_t _maxKickCount;
	int32_t _gateLostTimeout;
};

#endif //__ACCOUNT_H__
