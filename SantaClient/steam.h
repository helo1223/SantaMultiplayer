#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <steam/steam_api.h>
#include <string>
#include <windows.h>
#include <iostream>

extern HMODULE hSteam;
extern bool gotSteamName;
extern std::string g_steamName;

void TryGrabSteamName();
void GetPlayerName();