#pragma once
#include "global.h"
#include <d3d9.h>

#define IDX_ENDSCENE 42
#define IDX_DIP 82

typedef HRESULT(__stdcall* DrawIndexedPrimitive_t)(
    IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);

extern DrawIndexedPrimitive_t oDrawIndexedPrimitive;
extern EndScene_t oEndScene;

HRESULT __stdcall hkDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount);

HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev);

void** GetDeviceVTable();
bool InitializeHooks();