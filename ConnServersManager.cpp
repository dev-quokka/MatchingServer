#include "ConnServersManager.h"

// ================== CONNECTION SERVER MANAGEMENT ==================

void ConnServersManager::InsertServer(uint16_t connObjNum_, ConnServer* connServer_) {
	connServers[connObjNum_] = connServer_;
};

ConnServer* ConnServersManager::FindServer(uint16_t connObjNum_) {
	return connServers[connObjNum_];
};


// =============== CONNECTION GAME SERVER MANAGEMENT ================

bool ConnServersManager::CheckGameServerObjNum(uint16_t idx_) {
	if (gameServerObjNums[idx_] != 0) return false;
	return true;
}

void ConnServersManager::SetGameServerObjNum(uint16_t idx_, uint16_t gameServerObjNums_) {
	gameServerObjNums[idx_] = gameServerObjNums_;
}

ConnServer* ConnServersManager::GetGameServerObjNum(uint16_t idx_) {
	return connServers[gameServerObjNums[idx_]];
}