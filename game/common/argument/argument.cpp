#include "argument.h"

void Argument::Parse(int32_t argc, char ** argv) {
	int32_t pos = 0;
	while (pos < argc) {
		char * str = argv[pos];
		if (strncmp(str, "--", 2) == 0) {
			std::string name;
			std::string value;

			char * end = strstr(str + 2, "=");
			if (end) {
				name.append(str + 2, end);
				value = end + 1;
			}
			else
				name = str + 2;

			auto itr = _args.find(name);
			if (itr != _args.end()) {
				if (itr->second.hasDefault || itr->second.type == AT_BOOL || !value.empty()) {
					if (itr->second.type == AT_BOOL) {
						if (value != "" && value != "false" && value != "true")
							throw std::logic_error("bool argument is not valid value");

						itr->second.has = true;
						(*(bool*)itr->second.ptr) = (value == "false");
					}
					else if (!value.empty()) {
						itr->second.has = true;
						switch (itr->second.type) {
						case AT_INT8: (*(int8_t*)itr->second.ptr) = (int8_t)atoi(value.c_str()); break;
						case AT_INT16: (*(int16_t*)itr->second.ptr) = (int16_t)atoi(value.c_str()); break;
						case AT_INT32: (*(int32_t*)itr->second.ptr) = (int32_t)atoi(value.c_str()); break;
						case AT_INT64: (*(int64_t*)itr->second.ptr) = strtoull(value.c_str(), nullptr, 10); break;
						case AT_FLOAT: (*(float*)itr->second.ptr) = (float)atof(value.c_str()); break;
						case AT_STRING: (*(std::string*)itr->second.ptr) = std::move(value); break;
						}
					}
				}
			}
		}
		else if (strncmp(str, "-", 1) == 0) {
			int32_t len = (int32_t)strlen(str);
			if (len < 2)
				throw std::logic_error("invalid argument");

			char s = str[1];
			std::string value;
			if (len > 2)
				value = str + 2;
			else {
				if (pos + 1 < argc) {
					char * str2 = argv[pos + 1];
					if (strncmp(str2, "-", 1) != 0) {
						value = str2;
						++pos;
					}
				}
			}

			auto itr2 = _shortArgs.find(s);
			if (itr2 != _shortArgs.end()) {
				auto itr = _args.find(itr2->second);
				if (itr != _args.end()) {
					if (itr->second.hasDefault || itr->second.type == AT_BOOL || !value.empty()) {
						if (itr->second.type == AT_BOOL) {
							if (value != "" && value != "false" && value != "true")
								throw std::logic_error("bool argument is not valid value");

							itr->second.has = true;
							(*(bool*)itr->second.ptr) = (value == "false");
						}
						else if (!value.empty()) {
							itr->second.has = true;
							switch (itr->second.type) {
							case AT_INT8: (*(int8_t*)itr->second.ptr) = (int8_t)atoi(value.c_str()); break;
							case AT_INT16: (*(int16_t*)itr->second.ptr) = (int16_t)atoi(value.c_str()); break;
							case AT_INT32: (*(int32_t*)itr->second.ptr) = (int32_t)atoi(value.c_str()); break;
							case AT_INT64: (*(int64_t*)itr->second.ptr) = strtoull(value.c_str(), nullptr, 10); break;
							case AT_FLOAT: (*(float*)itr->second.ptr) = (float)atof(value.c_str()); break;
							case AT_STRING: (*(std::string*)itr->second.ptr) = std::move(value); break;
							}
						}
					}
				}
			}
		}
		++pos;
	}

	for (auto itr = _args.begin(); itr != _args.end(); ++itr) {
		if (!itr->second.has) {
			if (itr->second.hasDefault) {
				itr->second.has = true;
				switch (itr->second.type) {
				case AT_INT8: (*(int8_t*)itr->second.ptr) = (int8_t)itr->second.defaultValueInt; break;
				case AT_INT16: (*(int16_t*)itr->second.ptr) = (int16_t)itr->second.defaultValueInt; break;
				case AT_INT32: (*(int32_t*)itr->second.ptr) = (int32_t)itr->second.defaultValueInt; break;
				case AT_INT64: (*(int64_t*)itr->second.ptr) = itr->second.defaultValueInt; break;
				case AT_FLOAT: (*(float*)itr->second.ptr) = itr->second.defaultValueFloat; break;
				case AT_STRING: (*(std::string*)itr->second.ptr) = itr->second.defaultValueString; break;
				case AT_BOOL: (*(bool*)itr->second.ptr) = itr->second.defaultValueBool; break;
				}
			}
			else {
				throw std::logic_error(itr->first + " has no defined");
			}
		}
	}
}

void Argument::RegArgument(const char * name, char s, int8_t& t) {
	Args args;
	args.type = AT_INT8;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int16_t& t) {
	Args args;
	args.type = AT_INT16;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int32_t& t) {
	Args args;
	args.type = AT_INT32;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int64_t& t) {
	Args args;
	args.type = AT_INT64;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, float& t) {
	Args args;
	args.type = AT_FLOAT;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, std::string& t) {
	Args args;
	args.type = AT_STRING;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, bool& t) {
	Args args;
	args.type = AT_BOOL;
	args.ptr = &t;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int8_t& t, int8_t defaultValue) {
	Args args;
	args.type = AT_INT8;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueInt = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int16_t& t, int16_t defaultValue) {
	Args args;
	args.type = AT_INT16;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueInt = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int32_t& t, int32_t defaultValue) {
	Args args;
	args.type = AT_INT32;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueInt = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, int64_t& t, int64_t defaultValue) {
	Args args;
	args.type = AT_INT64;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueInt = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, float& t, float defaultValue) {
	Args args;
	args.type = AT_FLOAT;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueFloat = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, std::string& t, const char * defaultValue) {
	Args args;
	args.type = AT_STRING;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueString = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}

void Argument::RegArgument(const char * name, char s, bool& t, bool defaultValue) {
	Args args;
	args.type = AT_BOOL;
	args.ptr = &t;
	args.hasDefault = true;
	args.defaultValueBool = defaultValue;

	_args[name] = args;
	if (s != 0)
		_shortArgs[s] = name;
}