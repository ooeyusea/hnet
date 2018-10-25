#ifndef __GAME_H__
#define __GAME_H__
#include "hnet.h"

class Game {
public:
	Game() {}
	~Game() {}

	bool Start();
	void Run();
	void Release();
	void Terminate();

private:
	int32_t _listenFd;
	hn_channel(int8_t, 1) _closeCh;
};

#endif //__GAME_H__
