#include "d3d.h"
#include "hooks.h"
#include "steam.h"
#include "gift_esp.h"
#include "players.h"

D3DXMATRIX g_view;
D3DXMATRIX g_proj;

ID3DXFont* g_pFont = nullptr;
D3DXVECTOR3 testWorld = { 0.0f, 0.0f, 0.91f };

IDirect3DVertexBuffer9* savedVB = nullptr;
IDirect3DIndexBuffer9* savedIB = nullptr;
UINT savedStride = 0;

static const Vertex cubeVerts[] =
{
    // Front face
    {-0.5f,-0.5f,-0.5f, 0xFFFF0000}, {0.5f,-0.5f,-0.5f, 0xFFFF0000},
    {0.5f, 0.5f,-0.5f, 0xFFFF0000}, {-0.5f, 0.5f,-0.5f, 0xFFFF0000},
    // Back face
    {-0.5f,-0.5f, 0.5f, 0xFF00FF00}, {0.5f,-0.5f, 0.5f, 0xFF00FF00},
    {0.5f, 0.5f, 0.5f, 0xFF00FF00}, {-0.5f, 0.5f, 0.5f, 0xFF00FF00}
};

static const WORD cubeIndices[] =
{
    // front
    0,1,2, 0,2,3,
    // back
    5,4,7, 5,7,6,
    // left
    4,0,3, 4,3,7,
    // right
    1,5,6, 1,6,2,
    // top
    3,2,6, 3,6,7,
    // bottom
    4,5,1, 4,1,0
};

D3DXVECTOR3 Vec3ToVector3(Vec3 vec3) {
    return D3DXVECTOR3{ vec3.x, vec3.y, vec3.z };
}

void DrawLine(IDirect3DDevice9* dev, float x1, float y1, float x2, float y2, D3DCOLOR color)
{
    struct Vertex { float x, y, z, rhw; DWORD c; };
    Vertex v[2] = { {x1, y1, 0.0f, 1.0f, color}, {x2, y2, 0.0f, 1.0f, color} };
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    dev->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(Vertex));
}

void DrawCross(IDirect3DDevice9* dev, float x, float y, float size, D3DCOLOR color)
{
    DrawLine(dev, x - size, y, x + size, y, color);
    DrawLine(dev, x, y - size, x, y + size, color);
}

void DrawCube(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw)
{
    D3DXMATRIX mScale, mTrans, mWorld, mRot;
    D3DXMatrixScaling(&mScale, size, size, size);
    D3DXMatrixTranslation(&mTrans, pos.x, pos.y, pos.z);
    D3DXMatrixRotationZ(&mRot, -yaw);
    mWorld = mScale * mRot * mTrans;

    dev->SetTransform(D3DTS_WORLD, &mWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    dev->SetFVF(FVF_VERTEX);

    dev->DrawIndexedPrimitiveUP(
        D3DPT_TRIANGLELIST,
        0,
        8,
        12,
        cubeIndices,
        D3DFMT_INDEX16,
        cubeVerts,
        sizeof(Vertex)
    );
}

void DrawTextSimple(IDirect3DDevice9* dev, float x, float y, D3DCOLOR color, const char* text)
{
    if (!g_pFont)
    {
        D3DXCreateFontA(
            dev,
            18,
            0,
            FW_BOLD,
            1,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Arial",
            &g_pFont
        );

    }

    if (!g_pFont) return;

    // Measure text size
    RECT calcRect = { 0, 0, 0, 0 };
    g_pFont->DrawTextA(nullptr, text, -1, &calcRect, DT_CALCRECT, color);

    int textWidth = calcRect.right - calcRect.left;
    int textHeight = calcRect.bottom - calcRect.top;

    RECT drawRect;
    drawRect.left = (LONG)(x - textWidth / 2.0f);
    drawRect.top = (LONG)(y - textHeight / 2.0f);
    drawRect.right = drawRect.left + textWidth;
    drawRect.bottom = drawRect.top + textHeight;

    g_pFont->DrawTextA(nullptr, text, -1, &drawRect, DT_NOCLIP, color);
}

void DrawSanta(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw)
{
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);

    for (int i = 0; i < 8; ++i)
    {
        dev->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
        dev->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DXMATRIX mScale, mTrans, mWorld, mRot, mRotX, mRotY, mRotZ;
    D3DXMatrixScaling(&mScale, size, size, size);
    D3DXMatrixTranslation(&mTrans, pos.x, pos.y, pos.z - 0.91f);
    D3DXMatrixRotationX(&mRotX, D3DXToRadian(90.f));
    D3DXMatrixRotationY(&mRotY, D3DXToRadian(0.f));
    D3DXMatrixRotationZ(&mRotZ, yaw);
    mRot = mRotX * mRotY * mRotZ;

    mWorld = mScale * mRot * mTrans;
    dev->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
    dev->SetTransform(D3DTS_WORLD, &mWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);
    dev->SetStreamSource(0, savedVB, 0, savedStride);
    dev->SetIndices(savedIB);

}

void updateCameraPositions() 
{
    g_camAt = { *(float*)(g_camPtr + offsets::CAMT_X),*(float*)(g_camPtr + offsets::CAMT_Y),*(float*)(g_camPtr + offsets::CAMT_Z) };
    g_camPos = { *(float*)(g_camPtr + offsets::CAM_X),*(float*)(g_camPtr + offsets::CAM_Y),*(float*)(g_camPtr + offsets::CAM_Z) };
}

void updateViewMatrix(IDirect3DDevice9* dev)
{
    updateCameraPositions();


    D3DXVECTOR3 eye = Vec3ToVector3(g_camPos);
    D3DXVECTOR3 up(0, 0, 1);
    D3DXVECTOR3 at = Vec3ToVector3(g_camAt);
    D3DXMatrixLookAtRH(&g_view, &eye, &at, &up);

    D3DXVECTOR3 dir = at - eye;
    float yawRad = atan2f(dir.x, dir.y);
    g_localPlayerYaw = yawRad;

    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);
    float fovY = D3DXToRadian(50.0f);
    float aspect = (float)vp.Width / (float)vp.Height;
    float zn = 0.05f;
    float zf = 120.0;

    D3DXMatrixPerspectiveFovRH(&g_proj, fovY, aspect, zn, zf);
}

bool ProjectWorldToScreen(IDirect3DDevice9* dev, const D3DXVECTOR3& world, D3DXVECTOR3& out)
{
    D3DVIEWPORT9 vp;
    if (FAILED(dev->GetViewport(&vp))) return false;

    D3DXMATRIX w;
    D3DXMatrixIdentity(&w);
    D3DXMATRIX wvp = w * g_view * g_proj;

    D3DXVECTOR4 clip;
    D3DXVec3Transform(&clip, &world, &wvp);
    if (clip.w <= 0.001f) return false;

    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;

    out.x = (vp.Width * 0.5f) * (ndcX + 1.0f);
    out.y = (vp.Height * 0.5f) * (1.0f - ndcY);
    out.z = clip.z * invW;
    return true;
}

void OnDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount)
{
    IDirect3DStateBlock9* stateBlock = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &stateBlock);
    if (stateBlock) stateBlock->Capture();

    if (NumVertices == 200 && PrimCount == 148)
    {

        float vsConsts[4 * 4];
        dev->GetVertexShaderConstantF(0, vsConsts, 4);
        float* m = &vsConsts[0];
        float wx = m[3], wy = m[7], wz = m[11];
        float yaw = atan2(m[2], m[0]);

        gifts::g_gifts.push_back({ Vec3{wx, wy, wz}, yaw });
    }

    if (NumVertices == 8426 && PrimCount == 14796) {
        D3DXVECTOR3 localPlayerWorld = Vec3ToVector3(g_localPlayerPos);
        D3DXVECTOR3 localPlayerScreen;
        D3DXVECTOR3 localNamePos = Vec3ToVector3(g_localPlayerPos);
        localNamePos.z += 1.5f;
        D3DXVECTOR3 nameScreen;
        ProjectWorldToScreen(dev, localPlayerWorld, localPlayerScreen);
        if (ProjectWorldToScreen(dev, localNamePos, nameScreen))
        {
            DrawTextSimple(dev, nameScreen.x, nameScreen.y,
                D3DCOLOR_ARGB(255, 255, 255, 255), g_steamName.c_str());
        }
        if (gifts::drawESP) {
            dev->SetVertexShader(nullptr);
            dev->SetPixelShader(nullptr);
            dev->SetTexture(0, nullptr);

            for (gifts::Gift g : gifts::g_gifts)
            {
                D3DXVECTOR3 world(g.pos.x, g.pos.z, g.pos.y);
                D3DXVECTOR3 screen;

                if (ProjectWorldToScreen(dev, world, screen))
                {
                    DrawCross(dev, screen.x, screen.y, 6.0f, D3DCOLOR_ARGB(255, 0, 255, 0));
                    DrawLine(dev, localPlayerScreen.x, localPlayerScreen.y, screen.x, screen.y, D3DCOLOR_ARGB(180, 255, 255, 255));
                    DrawCube(dev, world, 1.0f, g.yaw);

                }
            }
        }

        if (!savedVB && !savedIB)
        {
            IDirect3DVertexBuffer9* vb = nullptr;
            UINT offset = 0, stride = 0;
            dev->GetStreamSource(0, &vb, &offset, &stride);

            IDirect3DIndexBuffer9* ib = nullptr;
            dev->GetIndices(&ib);

            if (vb && ib)
            {
                savedVB = vb;  savedVB->AddRef();
                savedIB = ib;  savedIB->AddRef();
                savedStride = stride;
            }
        }

        if (savedVB && savedIB)
        {
            DrawSanta(dev, testWorld, 1.5f, 0.0f);
            oDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 8426, 0, 14796);

            for (auto& kv : g_otherPlayers)
            {
                const RemotePlayer& rp = kv.second;
                Vec3 playerPos = rp.pos;
                D3DXVECTOR3 santaWorld = Vec3ToVector3(playerPos);
                D3DXVECTOR3 namePos = Vec3ToVector3(playerPos);
                namePos.z += 1.5f;
                D3DXVECTOR3 nameScreen;
                if (ProjectWorldToScreen(dev, namePos, nameScreen))
                {
                    DrawTextSimple(dev, nameScreen.x, nameScreen.y,
                        D3DCOLOR_ARGB(255, 255, 255, 255), rp.name.c_str());
                }

                DrawSanta(dev, santaWorld, 1.5f, -rp.yaw);
                oDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 8426, 0, 14796);

            }
        }
    }
    if (stateBlock)
    {
        stateBlock->Apply();
        stateBlock->Release();
    }
}

void OnEndScene(IDirect3DDevice9* dev)
{
    if (!g_santaBase || !g_camPtr)
        return;

    gifts::CheckToggleKey();
    updatePlayerCoords();
    updateViewMatrix(dev);

    gifts::g_gifts.clear();
}