#include "gift_esp.h"
#include "d3d.h"
namespace gifts {

    bool drawESP = false;

    void CheckToggleKey()
    {
        if (GetAsyncKeyState(TOGGLE_KEY) & 1) {
            drawESP = !drawESP;
        }
    }

    void DrawGiftESP(IDirect3DDevice9* dev, D3DXVECTOR3 localPlayerScreen)
    {
        float vsConsts[4 * 4];
        dev->GetVertexShaderConstantF(0, vsConsts, 4);
        float* m = &vsConsts[0];
        float wx = m[3], wy = m[11], wz = m[7];
        float yaw = atan2(m[2], m[0]);

        D3DXVECTOR3 world(wx, wy, wz);
        D3DXVECTOR3 screen;

        if (ProjectWorldToScreen(dev, world, screen))
        {
            dev->SetVertexShader(nullptr);
            dev->SetPixelShader(nullptr);
            dev->SetTexture(0, nullptr);
            DrawCross(dev, screen.x, screen.y, 6.0f, D3DCOLOR_ARGB(255, 0, 255, 0));
            DrawLine(dev, localPlayerScreen.x, localPlayerScreen.y, screen.x, screen.y, D3DCOLOR_ARGB(180, 255, 255, 255));
            DrawCube(dev, world, 1.0f, yaw);

        }
    }
}