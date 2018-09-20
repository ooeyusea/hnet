#ifndef __util_h__
#define __util_h__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef WIN32
#ifndef _WINSOCK2API_
#include <WinSock2.h>
#else
#include <Windows.h>
#endif
#include <Shlwapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
void __OAssert(const char * file, int32_t line, const char * funname, const char * debug);
#ifdef __cplusplus
};
#endif

#define SafeSprintf snprintf

#ifdef _DEBUG
#define OASSERT(p, format, ...) { \
    char debug[4096]; \
    SafeSprintf(debug, sizeof(debug), format, ##__VA_ARGS__); \
    ((p) ? (void)0 : (void)__OAssert(__FILE__, __LINE__, __FUNCTION__, debug)); \
}
#endif

#ifdef WIN32
#define CSLEEP(t) Sleep(t)
#else
#define CSLEEP(t) usleep((t) * 1000)
#endif

#ifdef __cplusplus
extern "C" {
#endif
	inline void __OMemcpy(void * dst, const int32_t maxSize, const void * src, const int32_t size) {
		OASSERT(size <= maxSize, "memcpy out of range");

		const int32_t len = (size > maxSize ? maxSize : size);
		memcpy(dst, src, len);
	}

	inline void __OMemset(void * dst, const int32_t maxSize, const int32_t val, const int32_t size) {
		OASSERT(size <= maxSize, "memset out of range");

		const int32_t len = (size > maxSize ? maxSize : size);
		memset(dst, val, len);
	}

	inline void __OStrcpy(char * dst, const int32_t maxSize, const char * val) {
		int32_t size = (int32_t)strlen(val);
		const int32_t len = (size > maxSize - 1 ? maxSize - 1 : size);
		memcpy(dst, val, len);
		dst[len] = 0;
	}

#ifdef __cplusplus
};
#endif

#define SafeMemcpy __OMemcpy
#define SafeMemset __OMemset
#define SafeStrcpy __OStrcpy

#ifndef MAX_PATH
#define MAX_PATH 260
#endif //MAX_PATH

#ifdef __cplusplus
#include <functional>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <atomic>
#include <thread>
#include <memory>

#endif
#endif //__util_h__
