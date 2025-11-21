#include "steam.h"

HMODULE hSteam = nullptr;

bool gotSteamName = false;
std::string g_steamName;

void TryGrabSteamName()
{
    if (gotSteamName) return;

    const char* name = SteamFriends()->GetPersonaName();
    if (name && *name) {
        g_steamName = name;
        gotSteamName = true;
    }
    std::cout << "[Steam] Name: " << g_steamName << std::endl;
}

void GetPlayerName()
{
    if (!hSteam) {
        hSteam = GetModuleHandleA("steam_api.dll");
    }

    if (hSteam) {
        TryGrabSteamName();
    }
    if (!hSteam || g_steamName == "Noob") {
        char username[256];
        DWORD size = 256;
        if (GetUserNameA(username, &size)) {
            g_steamName = username;
        }
        else {
            g_steamName = "UnknownPlayer";
        }
    }
}
