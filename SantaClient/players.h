#pragma once
#include "global.h"
#include <unordered_map>
#include <string>

struct RemotePlayer {
    Vec3 pos;
    float yaw;
    std::string name;
};

extern std::unordered_map<unsigned, RemotePlayer> g_otherPlayers;

extern Vec3 g_localPlayerPos;
extern float g_localPlayerYaw;

void updatePlayerCoords();