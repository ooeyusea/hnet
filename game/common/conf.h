#ifndef __CONF_H__
#define __CONF_H__
#include "util.h"
#include "argument.h"
#include <string>

class Conf {
public:
	Conf() {
#ifndef WIN32
		Argument::Instance().RegArgument("conf", 0, _conf, "/etc/chief/conf.xml");
#else
		Argument::Instance().RegArgument("conf", 0, _conf, "etc\\chief\\conf.xml");
#endif
	}

	virtual ~Conf() {}

protected:
	std::string _conf;
};

class Envir {
public:
	Envir() {
#ifndef WIN32
		Argument::Instance().RegArgument("envir", 0, _envir, "./envir/");
#else
		Argument::Instance().RegArgument("envir", 0, _envir, ".\\envir\\");
#endif
	}

	virtual ~Envir() {}

protected:
	std::string _envir;
};

#endif //__CONF_H__
