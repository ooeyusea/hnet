#ifndef __SOCKET_HELPER_H__
#define __SOCKET_HELPER_H__
#include "util.h"

#define TIMEOUT 5000
struct SocketReader {
	template <typename T>
	void ReadType(int32_t fd, T& t) {
		int32_t offset = 0;
		while (offset < sizeof(T)) {
			int32_t ret = hn_recv(fd, ((char*)&t) + offset, sizeof(T) - offset);
			if (ret <= 0)
				throw std::logic_error("read block failed");
			else
				offset += ret;
		}
	}

	template <typename T, typename B>
	void ReadRest(int32_t fd, B& b) {
		static_assert(sizeof(T) <= sizeof(B), "T must small than B");

		int32_t offset = sizeof(T);
		while (offset < sizeof(B)) {
			int32_t ret = hn_recv(fd, ((char*)&b) + offset, sizeof(B) - offset);
			if (ret <= 0)
				throw std::logic_error("read block failed");
			else
				offset += ret;
		}
	}

	template <typename T, typename B>
	void ReadMessage(int32_t fd, T t, B& b) {
		static_assert(sizeof(T) <= sizeof(B), "T must small than B");

		int32_t offset = 0;
		while (offset < sizeof(T)) {
			int32_t ret = hn_recv(fd, ((char*)&b) + offset, sizeof(T) - offset);
			if (ret <= 0)
				throw std::logic_error("read block failed");
			else
				offset += ret;
		}

		if ((*(T*)&b) != t)
			throw std::logic_error("no expect message");
	}

	std::string ReadBlock(int32_t fd, int32_t size) {
		std::string data;
		data.resize(size, 0);

		int32_t offset = 0;
		while (offset < size) {
			int32_t ret = hn_recv(fd, ((char*)data.data()) + offset, size - offset);
			if (ret <= 0)
				throw std::logic_error("read block failed");
			else
				offset += ret;
		}
		return data;
	}
};

#endif //__SOCKET_HELPER_H__
