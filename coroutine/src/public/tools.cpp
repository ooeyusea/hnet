#include "tools.h"

namespace tools {
#ifndef WIN32
	const char * GetFileExt(const char * filename) {
		const char * dot = strrchr(filename, '.');
		if (!dot || dot == filename)
			return "";
		return dot;
	}
#endif

	void ListFileInDirection(const char * path, const char * extension, const std::function<void(const char *, const char *)> &f) {
#ifdef WIN32
		WIN32_FIND_DATA finder;

		char tmp[512] = { 0 };
		SafeSprintf(tmp, sizeof(tmp), "%s/*.*", path);

		HANDLE handle = FindFirstFile(tmp, &finder);
		if (INVALID_HANDLE_VALUE == handle)
			return;

		while (FindNextFile(handle, &finder)) {
			if (strcmp(finder.cFileName, ".") == 0 || strcmp(finder.cFileName, "..") == 0)
				continue;

			SafeSprintf(tmp, sizeof(tmp), "%s/%s", path, finder.cFileName);
			if (finder.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				ListFileInDirection(tmp, extension, f);
			else {
				if (0 == strcmp(extension, PathFindExtension(finder.cFileName))) {
					PathRemoveExtension(finder.cFileName);
					f(finder.cFileName, tmp);
				}
			}
		}
#else
		DIR * dp = opendir(path);
		if (dp == nullptr)
			return;

		struct dirent * dirp;
		while ((dirp = readdir(dp)) != nullptr) {
			if (dirp->d_name[0] == '.')
				continue;

			char tmp[256] = { 0 };
			SafeSprintf(tmp, sizeof(tmp), "%s/%s", path, dirp->d_name);

			struct stat st;
			if (stat(tmp, &st) == -1)
				continue;

			if (S_ISDIR(st.st_mode))
				ListFileInDirection(tmp, extension, f);
			else {
				if (0 == strcmp(extension, GetFileExt(dirp->d_name))) {
					char name[256];
					SafeSprintf(name, sizeof(name), "%s", dirp->d_name);
					char * dot = strrchr(name, '.');
					if (dot != nullptr)
						*dot = 0;
					f(name, tmp);
				}
			}
		}
#endif
	}
}

