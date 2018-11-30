#include "PlayerMgr.h"
#include "initializor.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "ObjectMgr.h"
#include "object_register.h"
#include "zone.h"
#include "eventdefine.h"
#include "event_manager.h"

#define MAX_ROLE_DATA 4096
#define RECOVER_TIMEOUT 6 * 60 * 1000
#define SAVE_INTERVAL 3 * 60 * 1000

PlayerMgr g_playerMgr;

PlayerMgr::PlayerMgr() {
	InitializorMgr::Instance().AddStep("PlayerMgr#Init", [this]() {
		return Initialize();
	});
}

bool PlayerMgr::Initialize() {
	Cluster::Instance().Get().Register(rpc_def::ACTIVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Active).Comit();
	Cluster::Instance().Get().Register(rpc_def::DEACTIVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Deactive).Comit();
	Cluster::Instance().Get().Register(rpc_def::DELIVER_MESSAGE).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::DealMessage).Comit();
	Cluster::Instance().Get().Register(rpc_def::KILL_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Kill).Comit();
	Cluster::Instance().Get().Register(rpc_def::TRIGGER_SAVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Save).Comit();
	Cluster::Instance().Get().Register(rpc_def::REMOVE_ACTOR).AddOrder(*this, &PlayerMgr::Order).AddCallback<128>(*this, &PlayerMgr::Remove).Comit();

	return true;
}

int32_t PlayerMgr::Active(int64_t actor, const std::string& userId, int32_t fd, int16_t gate) {
	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		int16_t old = obj->Get<object::Player::gate>();
		obj->Set<object::Player::gate>(gate);

		StopRecoverTimer(obj);

		if (old == 0)
			g_eventMgr.Do(event_def::PLAYER_CONNECT, *obj);
	}
	else {
		obj = g_objectMgr.Create<object::Player>(__FILE__, __LINE__, actor);
		g_eventMgr.Do(event_def::OBJECT_CREATE, *obj);
		g_eventMgr.Do(event_def::PLAYER_CREATE, *obj);

		obj->Set<object::Player::gate>(gate);

		int16_t id = Cluster::Instance().GetId();
		int16_t logic = Zone::Instance().Calc(node_def::LOGIC, ZONE_FROM_ID(id), obj->GetID());
		int32_t logicIdx = Cluster::Instance().ServiceId(node_def::LOGIC, ID_FROM_ZONE(ZONE_FROM_ID(id), logic));
		try {
			rpc_def::TestData<rpc_def::RoleData, int32_t, 0> data = Cluster::Instance().Get().Call(logicIdx)
				.Do<rpc_def::TestData<rpc_def::RoleData, int32_t, 0>, 128>(rpc_def::LOAD_ACTOR, actor, logic);

			if (!data.test && Read(obj, data.data)) {
				g_eventMgr.Do(event_def::OBJECT_DESTROY, *obj);
				g_eventMgr.Do(event_def::PLAYER_DESTROY, *obj);

				g_objectMgr.Recove(obj);

				return err_def::LOAD_ROLE_FAILED;
			}

			Read(obj, data.data);

			g_eventMgr.Do(event_def::PLAYER_LOAD_COMPLETE, *obj);
			g_eventMgr.Do(event_def::PLAYER_CONNECT, *obj);
		}
		catch (hn_rpc_exception) {

		}
	}
	return err_def::NONE;
}

void PlayerMgr::Deactive(int64_t actor) {
	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		obj->Set<object::Player::gate>(0);
		g_eventMgr.Do(event_def::PLAYER_DISCONNECT, *obj);

		StopSaveTimer(obj);
		SaveObject(obj, true);

		StartRecoverTimer(obj);
	}
}

void PlayerMgr::DealMessage(int64_t actor, hn_deliver_buffer buff) {
	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		if (buff.size >= sizeof(int16_t) * 2) {
			int16_t msgId = *(int16_t*)buff.buff;

			hn_istream stream(buff.buff + sizeof(int16_t) * 2, buff.size - sizeof(int16_t) * 2);
			hn_iachiver ar(stream, obj->Get<object::Player::version>());

			Deal(msgId, ar, *obj);
		}
	}
}

rpc_def::KickPlayerAck PlayerMgr::Kill(int64_t actor, int32_t reason) {
	rpc_def::KickPlayerAck ret;
	ret.test = true;

	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		ret.data.test = true;

		StopSaveTimer(obj);
		StopRecoverTimer(obj);
		Pack(obj, ret.data.data);

		g_eventMgr.Do(event_def::OBJECT_DESTROY, *obj);
		g_eventMgr.Do(event_def::PLAYER_DESTROY, *obj);
		g_objectMgr.Recove(obj);
	}
	return ret;
}

void PlayerMgr::StartSaveTimer(Object * obj) {
	hn_ticker * ticker = (hn_ticker *)obj->Get<object::Player::save_timer>();
	if (!ticker) {
		ticker = new hn_ticker(SAVE_INTERVAL, 1);

		obj->Set<object::Player::save_timer>((int64_t)ticker);

		int64_t actor = obj->GetID();
		hn_fork[ticker, actor]() {
			for (auto i : *ticker) {
				try {
					Cluster::Instance().Get().Call(hyper_net::JUST_CALL).Do<128>(rpc_def::TRIGGER_SAVE_ACTOR, actor);
				}
				catch (hn_rpc_exception) {

				}
			}

			delete ticker;
		};
	}
}

void PlayerMgr::StopSaveTimer(Object * obj) {
	hn_ticker * ticker = (hn_ticker *)obj->Get<object::Player::save_timer>();
	if (ticker) {
		ticker->Stop();

		obj->Set<object::Player::save_timer>(0);
	}
}

void PlayerMgr::Save(int64_t actor) {
	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		SaveObject(obj, false);
	}
}

bool PlayerMgr::SaveObject(Object * obj, bool remove) {
	int16_t id = Cluster::Instance().GetId();
	int16_t logic = Zone::Instance().Calc(node_def::LOGIC, ZONE_FROM_ID(id), obj->GetID());
	int32_t logicIdx = Cluster::Instance().ServiceId(node_def::LOGIC, ID_FROM_ZONE(ZONE_FROM_ID(id), logic));

	try {
		rpc_def::RoleData data;
		Pack(obj, data);

		Cluster::Instance().Get().Call(logicIdx).Do<MAX_ROLE_DATA, int64_t, const rpc_def::RoleData&>(rpc_def::SAVE_ACTOR, obj->GetID(), data, remove);
		return true;
	}
	catch (hn_rpc_exception) {
		
	}
	return false;
}

void PlayerMgr::Pack(Object * obj, rpc_def::RoleData& data) {
	data.name = obj->Get<object::Player::name>();

	event_def::Data info{ *obj, data };
	g_eventMgr.Do(event_def::PACK_DATA, info);
}

bool PlayerMgr::Read(Object * obj, rpc_def::RoleData& data) {
	obj->SetString<object::Player::name>(data.name.c_str());

	event_def::Data d{ *obj, data };
	return g_eventMgr.Judge(event_def::PARSE_DATA, d);
}

void PlayerMgr::StartRecoverTimer(Object * obj) {
	hn_ticker * ticker = (hn_ticker *)obj->Get<object::Player::recover_timer>();
	if (!ticker) {
		ticker = new hn_ticker(RECOVER_TIMEOUT, 1);

		obj->Set<object::Player::recover_timer>((int64_t)ticker);

		int64_t actor = obj->GetID();
		hn_fork [ticker, actor]() {
			for (auto i : *ticker) {
				try {
					Cluster::Instance().Get().Call(hyper_net::JUST_CALL).Do<128>(rpc_def::REMOVE_ACTOR, actor, (int64_t)ticker);
				}
				catch (hn_rpc_exception) {

				}
			}

			delete ticker;
		};
	}
}

void PlayerMgr::StopRecoverTimer(Object * obj) {
	hn_ticker * ticker = (hn_ticker *)obj->Get<object::Player::recover_timer>();
	if (ticker) {
		ticker->Stop();

		obj->Set<object::Player::recover_timer>(0);
	}
}

void PlayerMgr::Remove(int64_t actor, int64_t ticker) {
	Object * obj = g_objectMgr.FindObject(actor);
	if (obj) {
		if (obj->Get<object::Player::recover_timer>() == ticker) {
			StopSaveTimer(obj);
			SaveObject(obj, true);

			g_eventMgr.Do(event_def::OBJECT_DESTROY, *obj);
			g_eventMgr.Do(event_def::PLAYER_DESTROY, *obj);
			g_objectMgr.Recove(obj);
		}
	}
}
