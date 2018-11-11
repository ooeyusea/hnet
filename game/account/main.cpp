#include "hnet.h"
#include "Account.h"
#include "servernode.h"
#include "argument.h"

void start(int32_t argc, char ** argv) {
	Cluster::Instance();
	Account account;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		return;
	}

	if (account.Start()) {
		if (!Cluster::Instance().Start())
			return;

		account.Run();
		account.Release();
	}
}
