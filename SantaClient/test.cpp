#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include <vector>
#include <cmath>
#include "MinHook.h"
#include <fstream>
#include <sstream>
#include <enet/enet.h>
#include <unordered_map>
#include <steam/steam_api.h>


#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// =====================================================
// Globals
// =====================================================
typedef HRESULT(__stdcall* DrawIndexedPrimitive_t)(
    IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(__stdcall* SetVertexShaderConstantF_t)(
    IDirect3DDevice9*, UINT, const float*, UINT);

DrawIndexedPrimitive_t oDrawIndexedPrimitive = nullptr;
EndScene_t oEndScene = nullptr;
SetVertexShaderConstantF_t oSetVertexShaderConstantF = nullptr;

static HWND gameWindow = nullptr;
static WNDPROC oWndProc = nullptr;


static uintptr_t g_gameBase = 0;
static uintptr_t g_santaBase = 0;
static uintptr_t g_camPtr = 0;

static const uintptr_t OFF_PLAYER_ROOT = 0x0039BC5C;
static const uintptr_t OFF_CAM_ROOTPTR = 0x0039BC64;
static const uintptr_t PLAYER_CHAIN_OFFS[] = { 0x3C, 0x7C, 0x44, 0x0 };
static const uintptr_t CAMERA_CHAIN_OFFS[] = { 0x1B4 };

static const uintptr_t OFF_SANTA_X = 0x1B0;
static const uintptr_t OFF_SANTA_Z = 0x1B8;
static const uintptr_t OFF_SANTA_Y = 0x1C0;
static const uintptr_t OFF_CAM_ROT_YAW = 0x04;
static const uintptr_t OFF_CAM_X = 0x28;
static const uintptr_t OFF_CAM_Z = 0x2C;
static const uintptr_t OFF_CAM_Y = 0x30;
static const uintptr_t OFF_CAMT_X = 0x34;
static const uintptr_t OFF_CAMT_Z = 0x38;
static const uintptr_t OFF_CAMT_Y = 0x3C;

struct Vec3 {
    float x, y, z;
};

struct RecordedPosition {
    Vec3 pos;
    float yaw;
};

struct Gift { float x, y, z, yaw; };
std::vector<Gift> g_gifts;

bool g_recording = false;
std::ofstream g_logFile;

bool g_replaying = false;
std::vector<RecordedPosition> g_recordedPositions;
size_t g_replayIndex = 0;



D3DXMATRIX g_view;
D3DXMATRIX g_proj;


bool drawESP = false;

struct Vertex
{
    float x, y, z;
    DWORD color;
};
#define FVF_VERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

Vertex cubeVerts[] =
{
    // Front face
    {-0.5f,-0.5f,-0.5f, 0xFFFF0000}, {0.5f,-0.5f,-0.5f, 0xFFFF0000},
    {0.5f, 0.5f,-0.5f, 0xFFFF0000}, {-0.5f, 0.5f,-0.5f, 0xFFFF0000},
    // Back face
    {-0.5f,-0.5f, 0.5f, 0xFF00FF00}, {0.5f,-0.5f, 0.5f, 0xFF00FF00},
    {0.5f, 0.5f, 0.5f, 0xFF00FF00}, {-0.5f, 0.5f, 0.5f, 0xFF00FF00}
};

WORD cubeIndices[] =
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


static IDirect3DVertexBuffer9* savedVB = nullptr;
static IDirect3DIndexBuffer9* savedIB = nullptr;
static UINT savedStride = 0;

float px, py, pz, pyaw;

static ENetHost* g_client = nullptr;
static ENetPeer* g_serverPeer = nullptr;
static bool g_isConnected = false;

struct RemotePlayer {
    float x, y, z, yaw;
    std::string name;
};

std::unordered_map<unsigned, RemotePlayer> g_otherPlayers;

HMODULE hSteam = nullptr;

bool gotSteamName = false;
std::string g_steamName;

ID3DXFont* g_pFont = nullptr;


void TryGrabSteamName()
{
    if (gotSteamName) return;

    const char* name = SteamFriends()->GetPersonaName();
    if (name && *name) {
        g_steamName = name;
        gotSteamName = true;
        std::cout << "[Steam] Username: " << g_steamName << "\n";
    }
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_ACTIVATEAPP || msg == WM_ACTIVATE)
        return 0; // ignore deactivate

    if (msg == WM_KILLFOCUS)
        return 0; // ignore focus loss

    return CallWindowProc(oWndProc, hwnd, msg, wParam, lParam);
}

void HookWindow(HWND hwnd)
{
    oWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWL_WNDPROC, (LONG_PTR)HookedWndProc);
}

bool ENet_Connect(const char* ip, int port)
{
    if (enet_initialize() != 0) {
        std::cout << "[ENet] Failed to initialize\n";
        return false;
    }

    g_client = enet_host_create(NULL, 1, 2, 0, 0);
    if (!g_client) {
        std::cout << "[ENet] Failed to create ENet client host\n";
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, ip);
    address.port = port;

    g_serverPeer = enet_host_connect(g_client, &address, 2, 0);
    if (!g_serverPeer) {
        std::cout << "[ENet] No available peers for connection\n";
        return false;
    }

    ENetEvent event;
    if (enet_host_service(g_client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        std::cout << "[ENet] Connected to " << ip << ":" << port << "\n";
        g_isConnected = true;
        return true;
    }

    std::cout << "[ENet] Failed to connect\n";
    enet_peer_reset(g_serverPeer);
    return false;
}

void ENet_Send(const std::string& msg)
{
    if (!g_isConnected) return;

    ENetPacket* packet = enet_packet_create(
        msg.c_str(), msg.size() + 1,
        ENET_PACKET_FLAG_RELIABLE
    );

    enet_peer_send(g_serverPeer, 0, packet);
}

void ENet_Poll()
{
    if (!g_client) return;

    ENetEvent event;
    while (enet_host_service(g_client, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
        {
            std::string msg((char*)event.packet->data);

            // Example: SYNC 1234 1.0 2.0 3.0 1.57
            if (msg.rfind("SYNC", 0) == 0)
            {
                unsigned id;
                float x, y, z, yaw;
                char name[64];

                sscanf(msg.c_str(), "SYNC %u %f %f %f %f %s", &id, &x, &y, &z, &yaw, name);
                // ignore our own data
                if (id != GetCurrentProcessId()) {
                    g_otherPlayers[id] = { x, y, z, yaw };
                    g_otherPlayers[id].name = name;
                }
            }

            enet_packet_destroy(event.packet);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT:
            std::cout << "[ENet] Disconnected\n";
            g_isConnected = false;
            break;
        }
    }
}


void CheckToggleKey()
{

    if (GetAsyncKeyState(VK_F2) & 1)
    {
        if (g_isConnected) {
            ENet_Send("Hello from the injected DLL!");
        }
    }

    if (GetAsyncKeyState(VK_F9) & 1) // pressed once
    {
        g_recording = !g_recording;

        if (g_recording)
        {
            g_logFile.open("record.txt", std::ios::out | std::ios::trunc);

        }
        else
        {
            if (g_logFile.is_open())
            {
                g_logFile.close();
            }
        }
    }
    // F10: toggle replay
    if (GetAsyncKeyState(VK_F10) & 1)
    {
        g_replaying = !g_replaying;
        g_recording = false;
        if (g_logFile.is_open()) g_logFile.close();

        if (g_replaying)
        {
            g_recordedPositions.clear();
            std::ifstream in("record.txt");
            std::string line;

            while (std::getline(in, line))
            {
                if (line.rfind("pos=", 0) == 0)
                {
                    RecordedPosition rec;
                    Vec3 v{};
                    float yaw;
                    if (sscanf_s(line.c_str(), "pos=%f %f %f %f", &v.x, &v.y, &v.z, &yaw) == 4) {
                        rec.pos = v;
                        rec.yaw = yaw;
                        g_recordedPositions.push_back(rec);
                    }
                }
            }

            g_replayIndex = 0;
            printf("Loaded %zu recorded positions\n", g_recordedPositions.size());
        }
    }
    if (GetAsyncKeyState(VK_F1) & 1) {
        drawESP = !drawESP;
    }
}

// =====================================================
// Small D3D helper
// =====================================================
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

    // Build world matrix (translation + scale)
    D3DXMATRIX mScale, mTrans, mWorld, mRot;
    D3DXMatrixScaling(&mScale, size, size, size);
    D3DXMatrixTranslation(&mTrans, pos.x, pos.y, pos.z);
    D3DXMatrixRotationZ(&mRot, -yaw); // or RotationY depending on your up-axis
    mWorld = mScale * mRot * mTrans;

    dev->SetTransform(D3DTS_WORLD, &mWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME); // or D3DFILL_WIREFRAME
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
D3DXVECTOR3 testWorld = { 0.0f, 0.0f, 0.0f };
float testYaw = 0.0f;


D3DXVECTOR3 lastPos = { 0.f, 0.f, 0.f };
float lastYaw = 0.0f;

int fps = 900;

void DrawSanta(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw)
{
    D3DXVECTOR3 dir = pos - lastPos;
    if (yaw != 0.f) lastYaw = yaw;


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
    // Build world matrix (translation + scale)
    D3DXMATRIX mScale, mTrans, mWorld, mRot, mRotX, mRotY, mRotZ;
    D3DXMatrixScaling(&mScale, size, size, size);
    D3DXMatrixTranslation(&mTrans, pos.x, pos.y, pos.z-0.91f);
    D3DXMatrixRotationX(&mRotX, D3DXToRadian(90.f)); // example rotation
    D3DXMatrixRotationY(&mRotY, D3DXToRadian(0.f)); // example rotation
    D3DXMatrixRotationZ(&mRotZ, lastYaw); // or RotationY depending on your up-axis
    mRot = mRotX * mRotY * mRotZ;     // rotate around X first, then Y

    mWorld = mScale * mRot * mTrans;
    dev->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1); // match game’s vertex layout if you know it
    dev->SetTransform(D3DTS_WORLD, &mWorld);
    dev->SetTransform(D3DTS_VIEW, &g_view);
    dev->SetTransform(D3DTS_PROJECTION, &g_proj);
    dev->SetStreamSource(0, savedVB, 0, savedStride);
    dev->SetIndices(savedIB);

    oDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 8426, 0, 14796);

    lastPos = pos;
}

void DrawTextSimple(IDirect3DDevice9* dev, float x, float y, D3DCOLOR color, const char* text)
{
    if (!g_pFont) return;

    RECT rect;
    rect.left = (LONG)x;
    rect.top = (LONG)y;
    rect.right = rect.left + 300;
    rect.bottom = rect.top + 50;

    g_pFont->DrawTextA(NULL, text, -1, &rect, DT_NOCLIP, color);
}


// =====================================================
// Project world → screen using Camera
// =====================================================
bool ProjectWorldToScreen(IDirect3DDevice9* dev, const D3DXVECTOR3& world, D3DXVECTOR3& out)
{
    D3DVIEWPORT9 vp;
    if (FAILED(dev->GetViewport(&vp))) return false;

    D3DXMATRIX w; 
    D3DXMatrixIdentity(&w);
    D3DXMATRIX wvp = w * g_view *g_proj;

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

// =====================================================
// Pointer resolution
// =====================================================
static void TryResolveGamePointers()
{
    __try {
        if (!g_gameBase) {
            HMODULE mod = GetModuleHandleW(L"SantaClausInTrouble.exe");
            if (mod) {
                g_gameBase = reinterpret_cast<uintptr_t>(mod);
                std::cout << "[Overlay] Base = 0x" << std::hex << g_gameBase << std::dec << "\n";
            }
        }

        if (g_gameBase && !g_camPtr) {
            uintptr_t root = *(uintptr_t*)(g_gameBase + OFF_CAM_ROOTPTR);
            if (!root) return;
            uintptr_t p = root;

            for (auto o : CAMERA_CHAIN_OFFS) {
                p = *(uintptr_t*)(p + o);
                if (!p) return;
            }
            g_camPtr = p;
            std::cout << "[Overlay] Cam = 0x" << std::hex << g_camPtr << std::dec << "\n";
        }

        if (g_gameBase && !g_santaBase) {
            uintptr_t root = *(uintptr_t*)(g_gameBase + OFF_PLAYER_ROOT);
            if (!root) return;
            uintptr_t p = root;
            for (auto o : PLAYER_CHAIN_OFFS) {
                p = *(uintptr_t*)(p + o);
                if (!p) return;
            }
            g_santaBase = p;
            std::cout << "[Overlay] Santa = 0x" << std::hex << g_santaBase << std::dec << "\n";
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // ignore crash, keep waiting
        Sleep(50);
    }
}




// =====================================================
// Hook: DrawIndexedPrimitive
// =====================================================
HRESULT __stdcall hkDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount)
{
    HRESULT result = oDrawIndexedPrimitive(dev, Type, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimCount);

    // --- setup simple 2D draw state ---
    IDirect3DStateBlock9* stateBlock = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &stateBlock);
    if (stateBlock) stateBlock->Capture();

    // Detect gift mesh
    if (NumVertices == 200 && PrimCount == 148)
    {

        float vsConsts[4 * 4];
        dev->GetVertexShaderConstantF(0, vsConsts, 4);
        float* m = &vsConsts[0];
        float wx = m[3], wy = m[7], wz = m[11];
        float yaw = atan2(m[2], m[0]);

        g_gifts.push_back({ wx, wy, wz, yaw });
    }


    if (NumVertices == 8426 && PrimCount == 14796) {
        D3DXVECTOR3 playerWorld = { px, py, pz };
        D3DXVECTOR3 playerScreen;
        D3DXVECTOR3 namePos(px, py, pz + 1.5f);
        D3DXVECTOR3 nameScreen;
        ProjectWorldToScreen(dev, playerWorld, playerScreen);
        if (ProjectWorldToScreen(dev, namePos, nameScreen))
        {
            DrawTextSimple(dev, nameScreen.x, nameScreen.y,
                D3DCOLOR_ARGB(255, 255, 255, 255), g_steamName.c_str());
        }
        if (drawESP) {
            dev->SetVertexShader(nullptr);
            dev->SetPixelShader(nullptr);
            dev->SetTexture(0, nullptr);

            for (auto& g : g_gifts)
            {
                D3DXVECTOR3 world(g.x, g.z, g.y);
                D3DXVECTOR3 screen;

                if (ProjectWorldToScreen(dev, world, screen))
                {
                    // bright cross marker
                    DrawCross(dev, screen.x, screen.y, 6.0f, D3DCOLOR_ARGB(255, 0, 255, 0));
                    // optional debug line from screen center (Santa/cam) to gift
                    DrawLine(dev, playerScreen.x, playerScreen.y, screen.x, screen.y, D3DCOLOR_ARGB(180, 255, 255, 255));
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
        else {
            if (savedVB && savedIB)
            {
                DrawSanta(dev, testWorld, 1.5f, 0.0f);

                for (auto& kv : g_otherPlayers)
                {
                    const RemotePlayer& rp = kv.second;

                    D3DXVECTOR3 santaWorld(rp.x, rp.y, rp.z);
                    D3DXVECTOR3 namePos(rp.x, rp.y, rp.z + 1.5f);
                    D3DXVECTOR3 nameScreen;
                    ProjectWorldToScreen(dev, playerWorld, playerScreen);
                    if (ProjectWorldToScreen(dev, namePos, nameScreen))
                    {
                        DrawTextSimple(dev, nameScreen.x, nameScreen.y,
                            D3DCOLOR_ARGB(255, 255, 255, 255), rp.name.c_str());
                    }

                    DrawSanta(dev, santaWorld, 1.5f, -rp.yaw);
                }
            }
        }
    }
    if (stateBlock)
    {
        stateBlock->Apply();
        stateBlock->Release();
    }

    return result;
}

void updatePlayerCoords()
{
    px = (float)*(double*)(g_santaBase + OFF_SANTA_X);
    py = (float)*(double*)(g_santaBase + OFF_SANTA_Y);
    pz = (float)*(double*)(g_santaBase + OFF_SANTA_Z);
}


void updateViewMatrix(IDirect3DDevice9* dev)
{
    // --- viewport center (for reference lines) ---
    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);
    float cx = vp.Width * 0.5f;
    float cy = vp.Height * 0.5f;

    float atX = *(float*)(g_camPtr + OFF_CAMT_X);
    float atY = *(float*)(g_camPtr + OFF_CAMT_Y);
    float atZ = *(float*)(g_camPtr + OFF_CAMT_Z);

    // --- read camera position ---
    float camX = *(float*)(g_camPtr + OFF_CAM_X);
    float camY = *(float*)(g_camPtr + OFF_CAM_Y);
    float camZ = *(float*)(g_camPtr + OFF_CAM_Z);

    D3DXVECTOR3 eye(camX, camY, camZ);
    D3DXVECTOR3 up(0, 0, 1);
    D3DXVECTOR3 at(atX, atY, atZ);
    D3DXMatrixLookAtRH(&g_view, &eye, &at, &up); // Right-handed (OpenGL-like)

    D3DXVECTOR3 dir = at - eye;
    float yawRad = atan2f(dir.x, dir.y);  // (X over Y, since Y is "forward")
    pyaw = yawRad;

    // Parameters
    float fovY = D3DXToRadian(50.0f); // FOV in radians
    float aspect = (float)vp.Width / (float)vp.Height;
    float zn = 0.05f;   // near plane
    float zf = 120.0; // far plane

    D3DXMatrixPerspectiveFovRH(&g_proj, fovY, aspect, zn, zf);
}

void doRecording()
{
    if (g_recording && g_logFile.is_open())
    {
        g_logFile << "pos=" << px << " " << py << " " << pz <<  " " << pyaw << "\n";
    }

    if (g_replaying && !g_recordedPositions.empty())
    {
        Vec3 pos = g_recordedPositions[g_replayIndex].pos;
        testWorld.x = pos.x;
        testWorld.y = pos.y;
        testWorld.z = pos.z;
        testYaw = g_recordedPositions[g_replayIndex].yaw;

        g_replayIndex++;
        if (g_replayIndex >= g_recordedPositions.size())
        {
            g_replayIndex = g_recordedPositions.size() - 1;
            g_replaying = false; // stop at end
            printf("Replay finished.\n");
        }
    }
    else {
        testWorld = { 0.f, 0.f, 0.91f };
    }
}

typedef BOOL(WINAPI* ClipCursor_t)(const RECT*);
ClipCursor_t oClipCursor;

BOOL WINAPI hkClipCursor(const RECT* rect)
{
    // block mouse confinement
    return TRUE;
}

typedef BOOL(WINAPI* SetCursorPos_t)(int, int);
SetCursorPos_t oSetCursorPos;

BOOL WINAPI hkSetCursorPos(int X, int Y)
{
    // block recentering
    return TRUE;
}

bool windowHooked = false;

// =====================================================
// Hook: EndScene
// =====================================================
HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev)
{
    ENet_Poll();

    if (!g_santaBase || !g_camPtr)
        return oEndScene(dev);

    if (!g_pFont)
    {
        D3DXCreateFontA(
            dev,                      // device
            18,                       // height
            0,                        // width
            FW_BOLD,                  // weight
            1,                        // miplevels
            FALSE,                    // italic
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Arial",
            &g_pFont
        );

        std::cout << "[ESP] Font created\n";
    }


    /*if (!gameWindow)
    {
        D3DDEVICE_CREATION_PARAMETERS p;
        dev->GetCreationParameters(&p);
        gameWindow = p.hFocusWindow;
    }
    else if (!windowHooked){
        //HookWindow(gameWindow);
        std::cout << "Window hooked - pausing disabled.\n";
        windowHooked = true;
    }*/



    IDirect3DSurface9* backBuf = nullptr;
    if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuf)))
        dev->SetRenderTarget(0, backBuf);

    CheckToggleKey();
    doRecording();


    updatePlayerCoords();
    updateViewMatrix(dev);

    if (g_isConnected) {
        char buf[128];
        sprintf(buf, "POS %.3f %.3f %.3f %.3f %u %s", px, py, pz, pyaw, GetCurrentProcessId(), g_steamName.c_str());
        ENet_Send(buf);
    }

    g_gifts.clear();


    return oEndScene(dev);
}


// =====================================================
// Hook setup
// =====================================================
void** GetDeviceVTable()
{
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return nullptr;

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetForegroundWindow();

    IDirect3DDevice9* dev = nullptr;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev)))
    {
        d3d->Release();
        return nullptr;
    }

    void** vtbl = *reinterpret_cast<void***>(dev);
    d3d->Release();
    dev->Release();
    return vtbl;
}

bool lastFramePressed = false;


DWORD WINAPI MainThread(LPVOID)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    std::cout << "[Overlay] DLL loaded\n";

    while (!g_gameBase || !g_camPtr || !g_santaBase) {
        TryResolveGamePointers();
    }

    if (MH_Initialize() != MH_OK)
        return 0;

    int tries = 0;
    void** vtbl = nullptr;
    while (!vtbl) {
        vtbl = GetDeviceVTable();
        tries++;
    }
    if (!vtbl) return 0;

    if (MH_CreateHook(vtbl[82], &hkDrawIndexedPrimitive, (void**)&oDrawIndexedPrimitive) == MH_OK)
        MH_EnableHook(vtbl[82]);
    if (MH_CreateHook(vtbl[42], &hkEndScene, (void**)&oEndScene) == MH_OK)
        MH_EnableHook(vtbl[42]);

    /*MH_CreateHook(&ClipCursor, hkClipCursor, (void**)&oClipCursor);
    MH_EnableHook(&ClipCursor);
    MH_CreateHook(&SetCursorPos, hkSetCursorPos, (void**)&oSetCursorPos);
    MH_EnableHook(&SetCursorPos);
    ShowCursor(true);*/
    std::cout << "[Overlay] Hooks active\n";

    // Wait for steam_api to load
    while (!hSteam) {
        hSteam = GetModuleHandleA("steam_api.dll");
        Sleep(100);
    }
    std::cout << "[Steam] Found steam_api module at " << hSteam << "\n";

    if (hSteam) {
        // Steam version detected
        TryGrabSteamName();
    }
    if (!hSteam || "Noob" == g_steamName) {
        char username[256];
        DWORD size = 256;
        if (GetUserNameA(username, &size)) {
            g_steamName = username;
        }
        else {
            g_steamName = "UnknownPlayer";
        }
    }
    ENet_Connect("REDACTED", 27015);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
