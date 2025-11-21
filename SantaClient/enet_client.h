#pragma once
#include "global.h"
#include <enet/enet.h>
#include "players.h"
#include <iostream>

namespace net {

	extern ENetHost* g_client;
	extern ENetPeer* g_serverPeer;
	extern bool g_isConnected;

	bool ENet_Connect(const char* ip, int port);
	void ENet_Send(const std::string& msg);
	void ENet_Poll();
	void ENet_UpdatePlayerPos();
	void ENet_Disconnect();

}