#include "hooks.h"
#include "enet_client.h"
#include "steam.h"

DWORD WINAPI MainThread(LPVOID)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);

    if (!InitializeHooks()) {
        std::cout << "[ERROR] Failed to initialize hooks\n";
        return 0;
    }

    GetPlayerName();

    net::ENet_Connect("127.0.0.1", 27015);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(mod);
        HANDLE hThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
