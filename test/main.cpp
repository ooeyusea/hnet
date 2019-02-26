#include "hnet.h"
#include <thread>
#include <string.h>
#include <string>
#include <algorithm>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>

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
	for (int32_t i = 0; i < 100; ++i) {
		hn_fork hn_stack(8 * 1024 * 1024) [] {
			hn_sleep 30000;
		};
	}

	hn_sleep 30000;
	printf("stack test complete\n");
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
		hn_fork[s] {
			getchar();
			hn_close(s);
		};

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

				//printf("recv %d %s\n", c, data.c_str());
				hn_send(c, data.c_str(), data.size());

				//hn_sleep 5000;
				hn_close(c);
			};
		}
	};
}

void test_random_data() {
	static char * g_data = nullptr;
	if (!g_data) {
		g_data = new char[1024 * 1024 + 1];
		for (int32_t i = 0; i < 1024 * 1024; ++i) {
			g_data[i] = 'a' + rand() % 26;
		}
		g_data[1024 * 1024 - 1] = '$';
		g_data[1024 * 1024] = 0;
	}

	static int32_t sizes[] = { 32, 128, 512, 1 * 1024, 4 * 1024, 16 * 1024, 64 * 1024, 256 * 1024, 1024 * 1024 };
	int32_t size = sizes[rand() % 9];

	hn_fork[size]{
		auto c = hn_connect("127.0.0.1", 9027);
		hn_send(c, g_data + (1024 * 1024 - size), size);
		//hn_send(c, g_end, 1);

		std::string data;
		char recvBuff[1024] = { 0 };
		int32_t recv = hn_recv(c, recvBuff, 1023);
		while (recv > 0) {
			recvBuff[recv] = 0;
			data += recvBuff;

			if (size < 128)
				printf("recv %s %d\n", recvBuff, recv);

			if (std::find(recvBuff, recvBuff + recv, '$') != recvBuff + recv)
				break;

			recv = hn_recv(c, recvBuff, 1023);
		}

		if (strncmp(data.c_str(), g_data + (1024 * 1024 - size), size) != 0) {
			printf("%d data not match %d %d %d\n", c, size, recv, (int32_t)data.size());
			if (size < 128) {
				printf("    %s\n", data.c_str());
				printf("    %s\n", g_data + (1024 * 1024 - size));
			}
		}

		//printf("recv %d %s\n", size, data.c_str());
		hn_close(c);
	};
}

void test_fix_data(int32_t size) {
	if (size > 1024 * 1024 - 1) {
		size = 1024 * 1024 - 1;
	}

	static char * g_data = nullptr;
	if (!g_data) {
		g_data = new char[1024 * 1024];
		for (int32_t i = 0; i < 1024 * 1024; ++i) {
			g_data[i] = 'a' + rand() % 26;
		}
		g_data[1024 * 1024 - 1] = 0;
		g_data[size - 1] = '$';
		g_data[size] = 0;
	}
	static char * g_end = "$";

	hn_fork [size]{
		auto c = hn_connect("127.0.0.1", 9027);
		hn_send(c, g_data, size);
		//hn_send(c, g_end, 1);

		std::string data;
		char recvBuff[1024] = { 0 };
		int32_t recv = hn_recv(c, recvBuff, 1023);
		while (recv > 0) {
			recvBuff[recv] = 0;
			data += recvBuff;

			if (std::find(recvBuff, recvBuff + recv, '$') != recvBuff + recv)
				break;

			//printf("%s %d\n", recvBuff, recv);
			recv = hn_recv(c, recvBuff, 1023);
		}

		if (strncmp(data.c_str(), g_data, size) != 0) {
			printf("%d data not match %d %d %d\n", c, size, recv, (int32_t)data.size());
			if (size < 128) {
				printf("    %s\n", data.c_str());
				printf("    %s\n", g_data);
			}
		}

		//printf("recv %d %s\n", size, data.c_str());
		hn_close(c);
	};
}

void test_timeout() {
	hn_fork[]{
		auto c = hn_connect("127.0.0.1", 9027);

		char recvBuff[1024] = { 0 };
		int32_t recv = hn_recv(c, recvBuff, 1023, 5000);
		printf("recv %d\n", recv);
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
	hn_channel<int, 10> ch1;
	
	hn_fork [ch1]{
		int a = 0;
		try {
			hn_sleep 5000;
			ch1 >> a;

			printf("a is %d\n", a);
		}
		catch (hyper_net::ChannelCloseException& e) {
			printf("%s\n", e.what());
		}
	};

	ch1.Close();
}

int32_t DoubleValue(int32_t v) {
	return v * 2;
}

void TestValue(int32_t v) {
	printf("test7\n");
}

class A {
public:
	template <typename AR>
	void Archive(AR & ar) const {
		ar << 2.0;
	}

	int32_t invoke() {
		return 222222;
	}
};

void test_rpc_server() {
	hn_rpc rpc;
	rpc.Register(1).AddCallback<128>(DoubleValue).Comit();
	rpc.Register(7).AddCallback<128>(TestValue).Comit();
	A a;
	rpc.Register(2).AddCallback<128>(a, &A::invoke).Comit();
	rpc.Register(3).AddCallback<128>([](int32_t a, int32_t b)->int32_t {
		return a + b;
	}).AddOrder([] ()-> int64_t {
		return 0;
	}).Comit();
	rpc.Register(4).AddCallback<128>([](int32_t c) {
		printf("c is %d\n", c);
	}).Comit();
	rpc.Register(5).AddCallback<128>([]() {
		printf("test5\n");
	}).Comit();

	rpc.Register(6).AddCallback<128>([]()->int32_t {
		return 6;
	}).Comit();

	//hyper_net::IsFunctor<decltype(DoubleValue)>::value;

	auto s = hn_listen("0.0.0.0", 9027);
	while (true) {
		auto c = hn_accept(s);
		if (c < 0)
			break;

		hn_fork[c, &rpc]{
			rpc.Attach(0, c);
		};
	}
}

void test_rpc_client() {
	auto c = hn_connect("127.0.0.1", 9027);
	if (c > 0) {
		hn_rpc rpc;
		rpc.Attach(0, c);
		int32_t i = rpc.Call(0).Do<int32_t, 128>(1, 1);
		int32_t j = rpc.Call(0).Do<int32_t, 128>(2);
		int32_t k = rpc.Call(0).Do<int32_t, 128>(3, 2, 3);
		rpc.Call(0).Do<128>(4, 4);
		rpc.Call(0).Do<128>(5);
		int32_t m = rpc.Call(0).Do<int32_t, 128>(6);
		try {
			int32_t n = rpc.Call(0).Do<int32_t, 128>(5);
		}
		catch (hn_rpc_exception & e) {
			printf("%s\n", e.what());
		}
		printf("i %d j %d k %d m %d\n", i, j, k, m);
	}
}

void test_echo_server() {
	hn_fork[]{
		auto s = hn_listen("0.0.0.0", 9027);
		while (true) {
			auto c = hn_accept(s);
			if (c < 0)
				break;

			hn_fork[c]{
				std::string data;
				char recvBuff[1024] = { 0 };
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

//32, 128, 512, 1 * 1024, 4 * 1024, 16 * 1024, 64 * 1024, 256 * 1024, 1 * 1024 * 1024

void test_echo_client() {
	static char * g_data = nullptr;
	if (!g_data) {
		g_data = new char[1024 * 1024];
		for (int32_t i = 0; i < 1024 * 1024; ++i) {
			g_data[i] = 'a' + rand() % 26;
		}
		g_data[1024 * 1024 - 1] = 0;
	}
	static char * g_end = "$";

	hn_fork[]{
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

			//printf("%s %d\n", recvBuff, recv);
			recv = hn_recv(c, recvBuff, 1023);
		}

		//printf("recv %s\n", data.c_str());
	};
}

void start(int32_t argc, char ** argv) {
	hn_info("case:{}", argv[1]);
	hn_debug("case:{}", argv[1]);
	hn_trace("case:{}", argv[1]);
	hn_warn("case:{}", argv[1]);
	if (strcmp(argv[1], "server") == 0)
		test_server();
	else if (strcmp(argv[1], "long") == 0)
		test_long_data(atoi(argv[2]));
	else if (strcmp(argv[1], "random") == 0) {
		int32_t count = atoi(argv[2]);
		for (int32_t i = 0; i < count; ++i)
			test_random_data();
	}
	else if (strcmp(argv[1], "fix") == 0) {
		int32_t count = atoi(argv[2]);
		int32_t size = atoi(argv[3]);
		for (int32_t i = 0; i < count; ++i)
			test_fix_data(size);
	}
	else if (strcmp(argv[1], "channel") == 0)
		test_channel();
	else if (strcmp(argv[1], "ticker") == 0)
		test_ticker();
	else if (strcmp(argv[1], "stack") == 0)
		test_stack();
	else if (strcmp(argv[1], "rpc_client") == 0)
		test_rpc_client();
	else if (strcmp(argv[1], "rpc_server") == 0)
		test_rpc_server();
	else if (strcmp(argv[1], "timeout") == 0)
		test_timeout();
}
