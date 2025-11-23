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

// ================= HIERARCHY TYPES ==================

struct D3DXFRAME_EX : public D3DXFRAME
{
    D3DXMATRIX CombinedMatrix;    // world-space combined
    D3DXMATRIX OriginalMatrix;    // original local transform from .x
};

struct D3DXMESHCONTAINER_EX : public D3DXMESHCONTAINER
{
    IDirect3DTexture9** ppTextures; // one per material
};

LPD3DXFRAME g_rootFrame = nullptr;
D3DXFRAME* g_flagFrame = nullptr;          // frame we’ll animate
LPD3DXANIMATIONCONTROLLER g_animController = nullptr;

class CAllocateHierarchy : public ID3DXAllocateHierarchy
{
public:
    IDirect3DDevice9* m_device;

    CAllocateHierarchy() : m_device(nullptr) {}

    void SetDevice(IDirect3DDevice9* dev) { m_device = dev; }

    STDMETHOD(CreateFrame)(LPCSTR Name, LPD3DXFRAME* ppNewFrame)
    {
        if (!ppNewFrame) return E_INVALIDARG;

        D3DXFRAME_EX* pFrame = new D3DXFRAME_EX;
        ZeroMemory(pFrame, sizeof(D3DXFRAME_EX));

        // Copy name
        if (Name)
        {
            size_t len = strlen(Name) + 1;
            pFrame->Name = (LPSTR)malloc(len);
            memcpy(pFrame->Name, Name, len);
        }
        else
        {
            pFrame->Name = nullptr;
        }

        D3DXMatrixIdentity(&pFrame->TransformationMatrix);
        D3DXMatrixIdentity(&pFrame->CombinedMatrix);
        D3DXMatrixIdentity(&pFrame->OriginalMatrix);

        *ppNewFrame = pFrame;
        return S_OK;
    }

    STDMETHOD(CreateMeshContainer)(
        LPCSTR Name,
        CONST D3DXMESHDATA* pMeshData,
        CONST D3DXMATERIAL* pMaterials,
        CONST D3DXEFFECTINSTANCE* pEffectInstances,
        DWORD NumMaterials,
        CONST DWORD* pAdjacency,
        LPD3DXSKININFO pSkinInfo,
        LPD3DXMESHCONTAINER* ppNewMeshContainer)
    {
        if (!ppNewMeshContainer) return E_INVALIDARG;
        if (!pMeshData || pMeshData->Type != D3DXMESHTYPE_MESH)
            return E_FAIL;

        LPD3DXMESH pMesh = pMeshData->pMesh;
        if (!pMesh) return E_FAIL;

        D3DXMESHCONTAINER_EX* pContainer = new D3DXMESHCONTAINER_EX;
        ZeroMemory(pContainer, sizeof(D3DXMESHCONTAINER_EX));

        // Copy name
        if (Name)
        {
            size_t len = strlen(Name) + 1;
            pContainer->Name = (LPSTR)malloc(len);
            memcpy(pContainer->Name, Name, len);
        }
        else
        {
            pContainer->Name = nullptr;
        }

        // Copy mesh
        pContainer->MeshData.Type = D3DXMESHTYPE_MESH;
        pContainer->MeshData.pMesh = pMesh;
        pMesh->AddRef();

        // Adjacency
        if (pAdjacency)
        {
            pContainer->pAdjacency = (DWORD*)malloc(sizeof(DWORD) * pMesh->GetNumFaces() * 3);
            memcpy(pContainer->pAdjacency, pAdjacency, sizeof(DWORD) * pMesh->GetNumFaces() * 3);
        }

        // Materials and textures
        pContainer->NumMaterials = max(NumMaterials, (DWORD)1);
        pContainer->pMaterials = (D3DXMATERIAL*)malloc(sizeof(D3DXMATERIAL) * pContainer->NumMaterials);
        pContainer->ppTextures = (IDirect3DTexture9**)malloc(sizeof(IDirect3DTexture9*) * pContainer->NumMaterials);
        ZeroMemory(pContainer->ppTextures, sizeof(IDirect3DTexture9*) * pContainer->NumMaterials);

        for (DWORD i = 0; i < pContainer->NumMaterials; ++i)
        {
            if (pMaterials && i < NumMaterials)
            {
                pContainer->pMaterials[i] = pMaterials[i];

                if (pMaterials[i].pTextureFilename && m_device)
                {
                    if (FAILED(D3DXCreateTextureFromFileA(m_device, pMaterials[i].pTextureFilename, &pContainer->ppTextures[i])))
                    {
                        D3DXCreateTextureFromFileA(m_device, "#tanne.dds", &pContainer->ppTextures[i]);
                    }
                }
            }
            else
            {
                ZeroMemory(&pContainer->pMaterials[i], sizeof(D3DXMATERIAL));
                pContainer->pMaterials[i].MatD3D.Diffuse = D3DXCOLOR(1, 1, 1, 1);
                pContainer->pMaterials[i].MatD3D.Ambient = D3DXCOLOR(1, 1, 1, 1);
            }
        }

        pContainer->pSkinInfo = nullptr; // waypoint is static, no skinning

        *ppNewMeshContainer = pContainer;
        return S_OK;
    }

    STDMETHOD(DestroyFrame)(LPD3DXFRAME pFrameToFree)
    {
        if (!pFrameToFree) return S_OK;

        D3DXFRAME_EX* pFrame = (D3DXFRAME_EX*)pFrameToFree;

        if (pFrame->Name)
            free(pFrame->Name);

        delete pFrame;
        return S_OK;
    }

    STDMETHOD(DestroyMeshContainer)(LPD3DXMESHCONTAINER pMeshContainerBase)
    {
        if (!pMeshContainerBase) return S_OK;

        D3DXMESHCONTAINER_EX* pContainer = (D3DXMESHCONTAINER_EX*)pMeshContainerBase;

        if (pContainer->Name)
            free(pContainer->Name);

        if (pContainer->MeshData.pMesh)
            pContainer->MeshData.pMesh->Release();

        if (pContainer->pAdjacency)
            free(pContainer->pAdjacency);

        if (pContainer->pMaterials)
            free(pContainer->pMaterials);

        if (pContainer->ppTextures)
        {
            for (DWORD i = 0; i < pContainer->NumMaterials; ++i)
            {
                if (pContainer->ppTextures[i])
                    pContainer->ppTextures[i]->Release();
            }
            free(pContainer->ppTextures);
        }

        if (pContainer->pSkinInfo)
            pContainer->pSkinInfo->Release();

        delete pContainer;
        return S_OK;
    }
};

CAllocateHierarchy g_alloc;
static bool g_waypointHierarchyLoaded = false;

// ================= HIERARCHY UTILS ==================

void InitFramesRecursive(D3DXFRAME* frame)
{
    if (!frame) return;

    D3DXFRAME_EX* fex = (D3DXFRAME_EX*)frame;
    fex->OriginalMatrix = fex->TransformationMatrix;

    // Try to auto-detect a "flag" frame by name
    if (frame->Name)
    {
        std::string name(frame->Name);

        if (name == "Box02") // flag mesh
        {
            g_flagFrame = frame;
            printf("Flag frame selected: %s\n", frame->Name);
        }
    }

    if (frame->pFrameSibling)
        InitFramesRecursive(frame->pFrameSibling);
    if (frame->pFrameFirstChild)
        InitFramesRecursive(frame->pFrameFirstChild);
}

void UpdateCombinedMatrices(D3DXFRAME* frame, const D3DXMATRIX* parentMatrix)
{
    if (!frame) return;

    D3DXFRAME_EX* fex = (D3DXFRAME_EX*)frame;

    if (parentMatrix)
        fex->CombinedMatrix = fex->TransformationMatrix * (*parentMatrix);
    else
        fex->CombinedMatrix = fex->TransformationMatrix;

    if (frame->pFrameSibling)
        UpdateCombinedMatrices(frame->pFrameSibling, parentMatrix);
    if (frame->pFrameFirstChild)
        UpdateCombinedMatrices(frame->pFrameFirstChild, &fex->CombinedMatrix);
}

void DrawMeshContainer(D3DXMESHCONTAINER_EX* pMeshCont, D3DXFRAME_EX* frame, IDirect3DDevice9* dev, const D3DXMATRIX& worldPlacement)
{
    if (!pMeshCont || !pMeshCont->MeshData.pMesh)
        return;

    D3DXMATRIX finalWorld = frame->CombinedMatrix * worldPlacement;
    dev->SetTransform(D3DTS_WORLD, &finalWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);

    for (DWORD i = 0; i < pMeshCont->NumMaterials; ++i)
    {
        dev->SetMaterial(&pMeshCont->pMaterials[i].MatD3D);
        dev->SetTexture(0, pMeshCont->ppTextures[i]);
        pMeshCont->MeshData.pMesh->DrawSubset(i);
    }
}

void DrawFrame(D3DXFRAME* frame, IDirect3DDevice9* dev, const D3DXMATRIX& worldPlacement)
{
    if (!frame) return;

    D3DXFRAME_EX* fex = (D3DXFRAME_EX*)frame;

    D3DXMESHCONTAINER_EX* pMeshCont = (D3DXMESHCONTAINER_EX*)frame->pMeshContainer;
    while (pMeshCont)
    {
        DrawMeshContainer(pMeshCont, fex, dev, worldPlacement);
        pMeshCont = (D3DXMESHCONTAINER_EX*)pMeshCont->pNextMeshContainer;
    }

    if (frame->pFrameSibling)
        DrawFrame(frame->pFrameSibling, dev, worldPlacement);
    if (frame->pFrameFirstChild)
        DrawFrame(frame->pFrameFirstChild, dev, worldPlacement);
}

// ================= LIGHTING ==================

void SetupModelLights(IDirect3DDevice9* dev)
{
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_ZENABLE, TRUE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);

    dev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(120, 120, 120));

    D3DLIGHT9 light{};
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse = D3DXCOLOR(1, 1, 1, 1);
    light.Direction = D3DXVECTOR3(0.f, 10.5f, 0.f);

    dev->SetLight(0, &light);
    dev->LightEnable(0, TRUE);
}

// ================= SIMPLE 2D / UTILS ==================

D3DXVECTOR3 Vec3ToVector3(Vec3 vec3) {
    return D3DXVECTOR3{ vec3.x, vec3.y, vec3.z };
}

void DrawLine(IDirect3DDevice9* dev, float x1, float y1, float x2, float y2, D3DCOLOR color)
{
    struct V { float x, y, z, rhw; DWORD c; };
    V v[2] = { {x1, y1, 0.0f, 1.0f, color}, {x2, y2, 0.0f, 1.0f, color} };
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    dev->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(V));
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

// ================= SANTA MODEL (unchanged) ==================

void DrawSanta(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw)
{
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    SetupModelLights(dev);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);

    D3DXMATRIX mScale, mTrans, mWorld, mRot, mRotX, mRotY, mRotZ;
    D3DXMatrixScaling(&mScale, -size, size, size);
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

    oDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 8426, 0, 14796);
}

// ================= CAMERA / VIEW ==================

void updateCameraPositions()
{
    g_camAt = { *(float*)(g_camPtr + offsets::CAMT_X),
                *(float*)(g_camPtr + offsets::CAMT_Y),
                *(float*)(g_camPtr + offsets::CAMT_Z) };
    g_camPos = { *(float*)(g_camPtr + offsets::CAM_X),
                 *(float*)(g_camPtr + offsets::CAM_Y),
                 *(float*)(g_camPtr + offsets::CAM_Z) };
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
    float zf = 120.0f;

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

// ================= WAYPOINT MODEL DRAW (HIERARCHY) ==================

void LoadWaypointModel(IDirect3DDevice9* dev)
{
    if (g_waypointHierarchyLoaded)
        return;

    g_alloc.SetDevice(dev);

    if (FAILED(D3DXLoadMeshHierarchyFromX(
        L"waypoint_000.x",
        D3DXMESH_MANAGED,
        dev,
        &g_alloc,
        nullptr,
        &g_rootFrame,
        &g_animController)))
    {
        std::cout << "[MODEL] Failed to load waypoint_000.x hierarchy\n";
        return;
    }

    std::cout << "[MODEL] Loaded waypoint_000.x hierarchy\n";

    InitFramesRecursive(g_rootFrame); // detect Box02 etc.

    if (g_animController)
    {
        DWORD animSetCount = g_animController->GetNumAnimationSets();
        if (animSetCount > 0)
        {
            LPD3DXANIMATIONSET animSet = nullptr;
            g_animController->GetAnimationSet(0, &animSet);

            g_animController->SetTrackAnimationSet(0, animSet);
            g_animController->SetTrackEnable(0, TRUE);
            g_animController->SetTrackWeight(0, 1.0f);
            g_animController->SetTrackSpeed(0, 1.0f);  // normal speed
        }
    }

    g_waypointHierarchyLoaded = true;
}

// Advances animation time continuously
void UpdateAnimation()
{
    if (!g_animController) return;

    static double lastTime = GetTickCount64() / 1000.0;
    double current = GetTickCount64() / 1000.0;
    double delta = current - lastTime;
    lastTime = current;

    g_animController->AdvanceTime(delta, nullptr);
}

void DrawTestModel(IDirect3DDevice9* dev)
{
    if (!g_waypointHierarchyLoaded)
    {
        LoadWaypointModel(dev);
    }

    UpdateAnimation();  // advance animation

    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    SetupModelLights(dev);

    // Base transform (scale, rotate, translate into the world)
    D3DXMATRIX scale, trans, rotX, worldPlacement;
    D3DXMatrixScaling(&scale, 0.055f, 0.055f, 0.055f);
    D3DXMatrixTranslation(&trans, 1.f, 2.f, 0.f);
    D3DXMatrixRotationX(&rotX, D3DXToRadian(90.f));

    worldPlacement = scale * rotX * trans;

    // Update matrices recursively starting at root
    UpdateCombinedMatrices(g_rootFrame, nullptr);

    // Draw recursively
    DrawFrame(g_rootFrame, dev, worldPlacement);
}

// ================= HOOKS / MAIN DRAW ==================

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

    if (vb && ib)
    {
        savedVB = vb; savedVB->AddRef();
        savedIB = ib; savedIB->AddRef();
        savedStride = stride;
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