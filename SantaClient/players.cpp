#include "players.h"

Vec3 g_localPlayerPos = { 0.f,0.f,0.f };
float g_localPlayerYaw = 0.f;


std::unordered_map<unsigned, RemotePlayer> g_otherPlayers;

void updatePlayerCoords()
{
    if (!g_santaBase) 
        return;

    g_localPlayerPos = {
        (float)*(double*)(g_santaBase + offsets::SANTA_X),
        (float)*(double*)(g_santaBase + offsets::SANTA_Y),
        (float)*(double*)(g_santaBase + offsets::SANTA_Z)
    };
}