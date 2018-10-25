#include "hnet.h"
#include "Game.h"

void start(int32_t argc, char ** argv) {
	Game game;
	if (game.Start())
		game.Run();
	game.Release();
}
