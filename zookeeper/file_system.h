#ifndef __FILE_SYSTEM_H__
#define __FILE_SYSTEM_H__
#include <string.h>
#include <string>
#include <vector>
#include <functional>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#ifdef WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

namespace olib {
	class FileFinder {
	public:
		FileFinder() {}
		~FileFinder() {}

		void Search(const std::string& pattern, const std::function<void(const std::string& file)>& fn) {
			std::string::size_type star = pattern.rfind('*');
			if (star != std::string::npos) {
				std::string::size_type slash = pattern.rfind(SLASH, star);
				std::string directory = (slash == std::string::npos ? "./" : pattern.substr(0, slash + 1));
				std::string subpattern = (slash == std::string::npos ? pattern : pattern.substr(slash + 1));

				Search(directory, subpattern, fn);
			}
			else
				fn(pattern);
		}

	private:
		void Search(const std::string& path, const std::string& pattern, const std::function<void(const std::string& file)>& fn) {
			if (pattern == "")
				fn(path);
			else {
				std::string::size_type slash = pattern.find(SLASH);
				std::string check = (slash == std::string::npos ? pattern : pattern.substr(0, slash));
				std::string subpattern = (slash == std::string::npos ? "" : pattern.substr(slash + 1));
				std::vector<std::string> cuts = Cut(check, '*');

				for (auto& p : fs::directory_iterator(path)) {
					if (slash != std::string::npos) {
						if (fs::is_directory(p.path()) && Check(p.path().filename().string(), cuts))
							Search(p.path().string(), subpattern, fn);
					}
					else {
						if (!fs::is_directory(p.path()) && Check(p.path().filename().string(), cuts))
							fn(p.path().string());
					}
				}
			}
		}

		std::vector<std::string> Cut(const std::string& str, char c) {
			std::vector<std::string> ret;

			std::string::size_type offset = 0;
			while (offset != std::string::npos) {
				std::string::size_type pos = str.find(c, offset);
				if (pos == std::string::npos) {
					ret.push_back(str.substr(offset));

					offset = std::string::npos;
				}
				else {
					ret.push_back(str.substr(offset, pos - offset));

					offset = pos + 1;
				}
			}

			return ret;
		}

		bool Check(const std::string& str, const std::vector<std::string>& pattern) {
			std::string::size_type offset = 0;
			for (auto& p : pattern) {
				if (p != "") {
					std::string::size_type pos = str.find(p, offset);
					if (pos == std::string::npos)
						return false;

					offset = pos + p.size();
				}
			}

			return true;
		}
	};
}

#endif //__FILE_SYSTEM_H__
