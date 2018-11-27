#include "PlayerMgr.h"
#include "object_holder.h"
#include "initializor.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "errordefine.h"

PlayerMgr g_playerMgr;

PlayerMgr::PlayerMgr() {
	Holder<PlayerMgr> take(this);

	InitializorMgr::Instance().AddStep("PlayerMgr#Init", [this]() {
		return Init();
	});
}

bool PlayerMgr::Init() {
	Cluster::Instance().Get().Register(rpc_def::ACTIVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Active).Comit();
	Cluster::Instance().Get().Register(rpc_def::DEACTIVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Deactive).Comit();
	Cluster::Instance().Get().Register(rpc_def::DELIVER_MESSAGE).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::DealMessage).Comit();
	Cluster::Instance().Get().Register(rpc_def::KILL_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Kill).Comit();
	return true;
}

int32_t PlayerMgr::Active(int64_t actor, const std::string& userId, int32_t fd, int16_t gate) {
	return err_def::NONE;
}

void PlayerMgr::Deactive(int64_t actor) {

}

void PlayerMgr::DealMessage(int64_t actor, hn_deliver_buffer buff) {

}

rpc_def::TestData<rpc_def::RoleData, bool, true> PlayerMgr::Kill(int64_t actor, int32_t reason) {
	rpc_def::TestData<rpc_def::RoleData, bool, true> ret;
	return ret;
}
