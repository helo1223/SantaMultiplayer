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
IDirect3DTexture9* savedSantaTexture = nullptr;

UINT savedStride = 0;

LPD3DXMESH g_testMesh = nullptr;
DWORD g_numMaterials = 0;
std::vector<IDirect3DTexture9*> g_textures;
std::vector<D3DMATERIAL9> g_materials;
static bool modelLoaded = false;

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

void LoadTestModel(IDirect3DDevice9* dev)
{
    LPD3DXBUFFER materialBuffer = nullptr;

    if (FAILED(D3DXLoadMeshFromX(
        L"tiny.x",
        D3DXMESH_MANAGED,
        dev,
        NULL,
        &materialBuffer,
        NULL,
        &g_numMaterials,
        &g_testMesh)))
    {
        std::cout << "[MODEL] Failed to load tiny.x\n";
        return;
    }

    std::cout << "[MODEL] Loaded tiny.x successfully (" << g_numMaterials << " materials)\n";

    D3DXMATERIAL* mats = (D3DXMATERIAL*)materialBuffer->GetBufferPointer();
    g_materials.resize(g_numMaterials);
    g_textures.resize(g_numMaterials);

    for (DWORD i = 0; i < g_numMaterials; i++)
    {
        g_materials[i] = mats[i].MatD3D;
        g_materials[i].Ambient = g_materials[i].Diffuse;

        if (mats[i].pTextureFilename)
        {
            if (FAILED(D3DXCreateTextureFromFileA(dev, mats[i].pTextureFilename, &g_textures[i])))
            {
                D3DXCreateTextureFromFileA(dev, "Tiny_skin.dds", &g_textures[i]);
            }
        }
        else
        {
            D3DXCreateTextureFromFileA(dev, "Tiny_skin.dds", &g_textures[i]);
        }
    }

    materialBuffer->Release();
}


// ================= LIGHTING ==================
void SetupModelLights(IDirect3DDevice9* dev)
{
    dev->SetRenderState(D3DRS_LIGHTING, TRUE);
    dev->SetRenderState(D3DRS_ZENABLE, TRUE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);

    dev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(120, 120, 120));

    // directional light
    D3DLIGHT9 light{};
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse = D3DXCOLOR(1, 1, 1, 1);
    light.Direction = D3DXVECTOR3(0.5f, -0.3f, 1.0f);

    dev->SetLight(0, &light);
    dev->LightEnable(0, TRUE);
}


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
    SetupModelLights(dev);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);


    D3DXMATRIX mScale, mTrans, mWorld, mRot, mRotX, mRotY, mRotZ;
    D3DXMatrixScaling(&mScale, size, size, size);
    D3DXMatrixTranslation(&mTrans, pos.x, pos.y, pos.z - 0.91f);
    D3DXMatrixRotationX(&mRotX, D3DXToRadian(90.f));
    D3DXMatrixRotationY(&mRotY, D3DXToRadian(0.f));
    D3DXMatrixRotationZ(&mRotZ, yaw);
    mRot = mRotX * mRotY * mRotZ;

    mWorld = mScale * mRot * mTrans;
    dev->SetTransform(D3DTS_WORLD, &mWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);
    dev->SetStreamSource(0, savedVB, 0, savedStride);
    dev->SetIndices(savedIB);

    if (savedSantaTexture)
        dev->SetTexture(0, savedSantaTexture);

    oDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 8426, 0, 14796);
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


void DrawTestModel(IDirect3DDevice9* dev)
{
    if (!modelLoaded) {
        LoadTestModel(dev);
        modelLoaded = true;
    }
    if (!g_testMesh) return;

    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    SetupModelLights(dev);


    D3DXMATRIX scale, trans, world;
    D3DXMatrixScaling(&scale, 0.006f, 0.006f, 0.006f);
    D3DXMatrixTranslation(&trans, 1.f, 1.f, 1.5f); // adjust position
    world = scale * trans;

    dev->SetTransform(D3DTS_WORLD, &world);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);

    for (DWORD i = 0; i < g_numMaterials; i++)
    {
        dev->SetMaterial(&g_materials[i]);
        dev->SetTexture(0, g_textures[i]);
        g_testMesh->DrawSubset(i);
    }
}

bool testDrawn = false;

void OnDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount)
{
    IDirect3DSurface9* surf = nullptr;
    dev->GetRenderTarget(0, &surf);
    if (NumVertices == 200 && PrimCount == 148)
    {
        if (gifts::drawESP) {
            IDirect3DStateBlock9* stateBlock = nullptr;
            dev->CreateStateBlock(D3DSBT_ALL, &stateBlock);
            if (stateBlock) stateBlock->Capture();
            gifts::DrawGiftESP(dev);
            if (stateBlock)
            {
                stateBlock->Apply();
                stateBlock->Release();
            }
        }
    }

    if (surf)
    {
        D3DSURFACE_DESC desc;
        surf->GetDesc(&desc);
        surf->Release();

        bool isBackBuffer = desc.Format == D3DFMT_A8R8G8B8;
        if (isBackBuffer)
        {
            if (NumVertices == 8426 && PrimCount == 14796) {
                D3DXVECTOR3 localNamePos = Vec3ToVector3(g_localPlayerPos);
                localNamePos.z += 1.5f;
                D3DXVECTOR3 nameScreen;
                if (ProjectWorldToScreen(dev, localNamePos, nameScreen))
                {
                    DrawTextSimple(dev, nameScreen.x, nameScreen.y,
                        D3DCOLOR_ARGB(255, 255, 255, 255), g_steamName.c_str());
                }

                if (!savedVB && !savedIB)
                {
                    StoreSantaModel(dev);
                }
                if (savedVB && savedIB)
                {
                    DrawSanta(dev, testWorld, 1.5f, 0.f);
                    DrawPlayers(dev);
                }            
            }
            if (!testDrawn) {
                testDrawn = true;
                DrawTestModel(dev);
            }
        }
    }
}

void StoreSantaModel(IDirect3DDevice9* dev)
{
        IDirect3DVertexBuffer9* vb = nullptr;
        UINT offset = 0, stride = 0;
        dev->GetStreamSource(0, &vb, &offset, &stride);

        IDirect3DIndexBuffer9* ib = nullptr;
        dev->GetIndices(&ib);

        // --- Store texture bound at stage 0 ---
        IDirect3DBaseTexture9* tex = nullptr;
        dev->GetTexture(0, &tex);

        if (vb && ib)
        {
            savedVB = vb; savedVB->AddRef();
            savedIB = ib; savedIB->AddRef();
            savedStride = stride;
        }

        if (tex)
        {
            savedSantaTexture = (IDirect3DTexture9*)tex;
            savedSantaTexture->AddRef();
            tex->Release();
        }

        if (vb) vb->Release();
        if (ib) ib->Release();
}

void DrawPlayers(IDirect3DDevice9* dev)
{
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
    }
}

void OnEndScene(IDirect3DDevice9* dev)
{
    if (!g_santaBase || !g_camPtr)
        return;

    gifts::CheckToggleKey();
    updatePlayerCoords();
    updateViewMatrix(dev);

    testDrawn = false;

}