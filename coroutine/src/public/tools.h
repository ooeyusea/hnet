#ifndef __TOOLS_H__
#define __TOOLS_H__
#include "util.h"
#ifndef WIN32
#include <libgen.h>
#endif
#include <chrono>

namespace tools {
    inline const char * GetAppPath() {
        static char * path = nullptr;

        if (nullptr == path) {
            path = NEW char[256];
            char link[256];
            memset(link, 0, 256);
            memset(path, 0, 256);

#ifdef WIN32
			GetModuleFileName(nullptr, path, 256);
			PathRemoveFileSpec(path);
#else
			SafeSprintf(link, sizeof(link), "/proc/self/exe");

            int nCount = readlink(link, path, 256);
            if (nCount >= 265) {
                OASSERT(false, "system path error");
            }
            path = dirname(path);
#endif
        }
        return path;
    }

	inline const char * GetWorkPath() {
		static char * path = nullptr;

		if (nullptr == path) {
			path = NEW char[256];
			char link[256];
			memset(link, 0, 256);
			memset(path, 0, 256);

#ifdef WIN32
			GetCurrentDirectory(256, path);
#else
			path = getcwd(path, 256);
#endif
		}
		return path;
	}

    inline s32 StringAsInt(const char * val) {
        OASSERT(val != nullptr, "val is null");
        return atoi(val);
    }

	inline s64 StringAsInt64(const char * val) {
		OASSERT(val != nullptr, "val is null");
		return atoll(val);
	}

	inline float StringAsFloat(const char * val) {
		OASSERT(val != nullptr, "val is null");
		return (float)atof(val);
	}

	inline bool StringAsBool(const char * val) {
		OASSERT(val != nullptr, "val is null");
		return strcmp(val, "true") == 0;
	}

    inline s64 GetTimeMillisecond() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

	inline s64 GetTimeNanosecond() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	inline s64 CalcStringUniqueId(const char * str) {
		s64 seed = 131;
		s64 hash = 0;
		while (*str) {
			hash = (hash * seed + (*str++)) % 4294967295;
		}
		return hash;
	}

	void ListFileInDirection(const char * path, const char * extension, const std::function<void (const char *, const char *)>& f);
}
#endif // __TOOLS_H__
