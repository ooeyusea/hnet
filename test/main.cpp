#include "hnet.h"
#include <thread>
#include <string.h>
#include <string>
#include <algorithm>

void test_async() {
	hn_fork[]{
		auto * queue = hn_create_async(1, false, [](void* data) {
			*static_cast<int32_t*>(data) = 1;
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		});

		hn_fork[queue]{
			int32_t a = 0;
			queue->Call(0, &a);
			printf("async test is %d\n", a);
		};
	};
}

void test_stack() {
	hn_fork hn_stack(1024 * 1024) [] {
		printf("stack test complete\n");
	};
}

void test_net() {
	hn_fork []{
		auto s = hn_listen("0.0.0.0", 9027);
		for (int32_t i = 0; i < 16; ++i) {
			hn_fork[s]{
				while (true) {
					auto c = hn_accept(s);
					if (c < 0)
						break;

					hn_fork[c]{
						char recvBuff[1024] = { 0 };
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
	};
}

void test_server() {
	hn_fork []{
		auto s = hn_listen("0.0.0.0", 9027);
		while (true) {
			auto c = hn_accept(s);
			if (c < 0)
				break;

			hn_fork[c]{
				std::string data;
				char recvBuff[1024] = {0};
				int32_t recv = hn_recv(c, recvBuff, 1023);
				while (recv > 0) {
					recvBuff[recv] = 0;
					data += recvBuff;

					if (std::find(recvBuff, recvBuff + recv, '$') != recvBuff + recv)
						break;

					//printf("%s %d\n", recvBuff, recv);
					recv = hn_recv(c, recvBuff, 1023);
				}

				//printf("recv %s\n", data.c_str());
				hn_send(c, data.c_str(), data.size());

				hn_sleep 5000;
				hn_close(c);
			};
		}
	};
}

void test_random_data() {
	static char * g_data = nullptr;
	if (!g_data) {
		g_data = new char[1024 * 1024];
		for (int32_t i = 0; i < 1024 * 1024; ++i) {
			g_data[i] = 'a' + rand() % 26;
		}
		g_data[1024 * 1024 - 1] = 0;
	}
	static char * g_end = "$";

	hn_fork []{
		auto c = hn_connect("127.0.0.1", 9027);
		hn_send(c, g_data + rand() % 1024 * 1022, rand() % 1024);
		hn_send(c, g_end, 1);

		std::string data;
		char recvBuff[1024] = { 0 };
		int32_t recv = hn_recv(c, recvBuff, 1023);
		while (recv > 0) {
			recvBuff[recv] = 0;
			data += recvBuff;

			if (std::find(recvBuff, recvBuff + recv, '$') != recvBuff + recv)
				break;

			printf("%s %d\n", recvBuff, recv);
			recv = hn_recv(c, recvBuff, 1023);
		}

		printf("recv %s\n", data.c_str());
	};
}

void test_long_data(int32_t len) {
	hn_fork [len]{
		char * data = new char[len + 1];
		for (int32_t i = 0; i < len; ++i) {
			data[i] = 'a' + rand() % 26;
		}
		data[len - 1] = '$';
		data[len] = 0;
		//printf("data:0x%x\n", (int64_t)data);

		auto c = hn_connect("127.0.0.1", 9027);
		hn_send(c, data, len);

		std::string str;
		char recvBuff[1024] = { 0 };
		int32_t recv = hn_recv(c, recvBuff, 1023);
		while (recv > 0) {
			recvBuff[recv] = 0;
			str += recvBuff;

			if (std::find(recvBuff, recvBuff + recv, '$') != recvBuff + recv)
				break;

			//printf("%s %d\n", recvBuff, recv);
			recv = hn_recv(c, recvBuff, 1023);
		}

		printf("recv %s\n", str.c_str());

		//printf("data:0x%x\n", (int64_t)data);
		delete[] data;

		//hn_sleep 5000;
	};
}

void test_ticker() {
	hn_fork[]{
		hn_ticker ticker(2000);
		hn_fork [&ticker]{
			for (auto i : ticker) {
				printf("ticker : %d\n", i);
			}
		};

		hn_sleep 7000;
		ticker.Stop();
	};
}

void test_mutex() {
	hn_mutex lock;
	hn_fork [&lock] {
		printf("pre lock\n");

		hn_sleep 2000;
		lock.lock();
		lock.unlock();

		printf("lock complete\n");
	};

	lock.lock();
	hn_sleep 10000;
	lock.unlock();
}

void test_active() {
	bool terminate = false;
	while (!terminate) {
		hn_wait 1;
		printf("active start in 1 min\n");
		hn_wait 2;
		printf("active started");


	}
}

void test_channel() {
	hn_channel(int, 10) ch1;
	hn_channel(int, 10) ch2;
	
	hn_fork [&ch1, &ch2]{
		int a = 0;
		try {
			ch1 >> a;


			printf("a is %d\n", a);
		}
		catch (hyper_net::ChannelCloseException& e) {
			printf("%s\n", e.what());
		}

		hn_sleep 5000;
		ch2 << (a + 1);
	};

	ch1.Close();

	int b = 0;
	ch2 >> b;
	printf("b is %d\n", b);
}

void start(int32_t argc, char ** argv) {
	if (strcmp(argv[1], "server") == 0)
		test_server();
	else if (strcmp(argv[1], "long") == 0)
		test_long_data(atoi(argv[2]));
	else if (strcmp(argv[1], "random") == 0) {
		int32_t count = atoi(argv[2]);
		for (int32_t i = 0; i < count; ++i)
			test_random_data();
	}
	else if (strcmp(argv[1], "channel") == 0)
		test_channel();
}
