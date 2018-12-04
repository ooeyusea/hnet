#include "hnet.h"
#include "Account.h"
#include "servernode.h"
#include "argument.h"
#include "zone.h"

void start(int32_t argc, char ** argv) {
	hn_info("Starting Account ......");
	Cluster::Instance().SetType(node_def::ACCOUNT);
	Account account;

	try {
		Argument::Instance().Parse(argc, argv);
	}
	catch (std::exception & e) {
		hn_error("read argv failed: {}", e.what());
		return;
	}

	hn_info("Account id is {}", Cluster::Instance().GetId());

	if (account.Start()) {
		if (!Zone::Instance().Start())
			return;

		if (!Cluster::Instance().Start())
			return;

		hn_info("Account {} Started", Cluster::Instance().GetId());

		account.Run();
		account.Release();

		hn_info("Account {} Stoped", Cluster::Instance().GetId());
	}
}
