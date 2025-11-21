#include "gift_esp.h"

namespace gifts {

    bool drawESP = false;
    std::vector<Gift> g_gifts;

    void CheckToggleKey()
    {
        if (GetAsyncKeyState(TOGGLE_KEY) & 1) {
            drawESP = !drawESP;
        }
    }

}