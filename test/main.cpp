#include "hnet.h"
#include <thread>

void start(int32_t argc, char ** argv) {
	hn_mutex lock;
	hn_ticker ticker(2000);
	hn_fork [&ticker, &lock]{
		hn_sleep 2000;
		lock.lock();
		lock.unlock();
		for (auto i : ticker) {
			printf("%d\n", i);
		}
	};

	auto s = hn_listen("0.0.0.0", 9027);
	for (int32_t i = 0; i < 16; ++i) {
		hn_fork [s]{
			while (true) {
				auto c = hn_accept(s);
				if (c < 0)
					break;

				hn_fork[c]{
					char recvBuff[1024] = {0};
					int32_t recv = hn_recv(c, recvBuff, 1023);

					if (recv > 0) {
						recvBuff[recv] = 0;
						printf("recv %s\n", recvBuff);
						hn_send(c, recvBuff, strlen(recvBuff));
					}

					hn_sleep 5000;
					hn_close(c);
				};
			}
		};
	}

	lock.lock();
	hn_sleep 10000;
	lock.unlock();
	hn_sleep 30000;
	ticker.Stop();
}
