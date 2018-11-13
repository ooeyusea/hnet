#ifndef __ARGUMENT_H__
#define __ARGUMENT_H__
#include "hnet.h"
#include <string>
#include <map>
#include <list>

class Argument {
	enum {
		AT_INT8,
		AT_INT16,
		AT_INT32,
		AT_INT64,
		AT_FLOAT,
		AT_STRING,
		AT_BOOL,
	};

	struct Args {
		int8_t type;
		void * ptr;
		bool has = false;

		bool hasDefault = false;
		bool defaultValueBool;
		float defaultValueFloat;
		int64_t defaultValueInt;
		std::string defaultValueString;
	};
public:
	static Argument& Instance() {
		static Argument g_instance;
		return g_instance;
	}

	void Parse(int32_t argc, char ** argv);

	void RegArgument(const char * name, char s, int8_t& t);
	void RegArgument(const char * name, char s, int16_t& t);
	void RegArgument(const char * name, char s, int32_t& t);
	void RegArgument(const char * name, char s, int64_t& t);
	void RegArgument(const char * name, char s, float& t);
	void RegArgument(const char * name, char s, std::string& t);
	void RegArgument(const char * name, char s, bool& t);

	void RegArgument(const char * name, char s, int8_t& t, int8_t defaultValue);
	void RegArgument(const char * name, char s, int16_t& t, int16_t defaultValue);
	void RegArgument(const char * name, char s, int32_t& t, int32_t defaultValue);
	void RegArgument(const char * name, char s, int64_t& t, int64_t defaultValue);
	void RegArgument(const char * name, char s, float& t, float defaultValue);
	void RegArgument(const char * name, char s, std::string& t, const char * defaultValue);
	void RegArgument(const char * name, char s, bool& t, bool defaultValue);

private:
	Argument() {}
	~Argument() {}

	std::map<std::string, std::list<Args>> _args;
	std::map<char, std::string> _shortArgs;
};

#endif // !__RPC_H__
