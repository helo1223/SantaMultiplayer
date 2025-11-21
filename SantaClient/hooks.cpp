#include "hooks.h"
#include "d3d.h"
#include "enet_client.h"
#include "MinHook.h"

DrawIndexedPrimitive_t oDrawIndexedPrimitive = nullptr;
EndScene_t oEndScene = nullptr;

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

bool InitializeHooks()
{
    while (!g_gameBase || !g_camPtr || !g_santaBase) {
        TryResolveGamePointers();
    }

    if (MH_Initialize() != MH_OK)
        return false;

    void** vtbl = nullptr;
    while (true) {
        vtbl = GetDeviceVTable();
        if (vtbl && IsBadReadPtr(vtbl, sizeof(void*) * 120) == FALSE)
            break;
        Sleep(100);
    }
    if (!vtbl) return false;

    if (MH_CreateHook(vtbl[IDX_DIP], &hkDrawIndexedPrimitive, (void**)&oDrawIndexedPrimitive) == MH_OK)
        MH_EnableHook(vtbl[IDX_DIP]);
    if (MH_CreateHook(vtbl[IDX_ENDSCENE], &hkEndScene, (void**)&oEndScene) == MH_OK)
        MH_EnableHook(vtbl[IDX_ENDSCENE]);

    return true;
}


HRESULT __stdcall hkDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount)
{
    HRESULT result = oDrawIndexedPrimitive(dev, Type, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimCount);

    IDirect3DStateBlock9* stateBlock = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &stateBlock);
    if (stateBlock) stateBlock->Capture();

    OnDrawIndexedPrimitive(dev, Type, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimCount);

    if (stateBlock)
    {
        stateBlock->Apply();
        stateBlock->Release();
    }

    return result;
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev)
{    
    if (net::g_isConnected) {
        net::ENet_Poll();
    }

    OnEndScene(dev);
    net::ENet_UpdatePlayerPos();

    return oEndScene(dev);
}