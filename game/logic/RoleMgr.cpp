#include "RoleMgr.h"
#include "util.h"
#include "servernode.h"
#include "nodedefine.h"
#include "rpcdefine.h"
#include "errordefine.h"
#include "zone.h"
#include "locktable.h"
#include "spin_mutex.h"
#include "servernode.h"
#include "rpcdefine.h"
#include "object_holder.h"
#include "dbdefine.h"

#define MAX_ROLE_DATA 4096
#define SAVE_INTERVAL (360 * 1000)
#define RECOVER_TIMEOUT (24 * 3600 * 1000)
#define MAX_LANDING_SQL_SIZE 65536

Role::Role() {

}

Role::~Role() {

}

void RoleMgr::Start() {
}
