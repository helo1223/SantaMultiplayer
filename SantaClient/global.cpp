#include "global.h"

uintptr_t g_gameBase = 0;
uintptr_t g_santaBase = 0;
uintptr_t g_camPtr = 0;

Vec3 g_camAt = { 0,0,0 };
Vec3 g_camPos = { 0,0,0 };

void TryResolveGamePointers()
{
    __try {
        if (!g_gameBase) {
            HMODULE mod = GetModuleHandleW(L"SantaClausInTrouble.exe");
            if (mod) {
                g_gameBase = reinterpret_cast<uintptr_t>(mod);
            }
        }

        if (g_gameBase && !g_camPtr) {
            uintptr_t root = *(uintptr_t*)(g_gameBase + offsets::CAM_ROOTPTR);
            if (!root) return;
            uintptr_t p = root;

            for (auto o : offsets::CAMERA_CHAIN) {
                p = *(uintptr_t*)(p + o);
                if (!p) return;
            }
            g_camPtr = p;
        }

        if (g_gameBase && !g_santaBase) {
            uintptr_t root = *(uintptr_t*)(g_gameBase + offsets::PLAYER_ROOT);
            if (!root) return;
            uintptr_t p = root;

            for (auto o : offsets::PLAYER_CHAIN) {
                p = *(uintptr_t*)(p + o);
                if (!p) return;
            }
            g_santaBase = p;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Sleep(50);
    }
}
