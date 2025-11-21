#pragma once
#include "global.h"
#include <d3d9.h>
#include <d3dx9.h>

#define FVF_VERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)

extern D3DXMATRIX g_view;
extern D3DXMATRIX g_proj;
extern D3DXVECTOR3 testWorld;
extern ID3DXFont* g_pFont;

extern IDirect3DVertexBuffer9* savedVB;
extern IDirect3DIndexBuffer9* savedIB;
extern UINT savedStride;

void DrawLine(IDirect3DDevice9* dev, float x1, float y1, float x2, float y2, D3DCOLOR color);
void DrawCross(IDirect3DDevice9* dev, float x, float y, float size, D3DCOLOR color);
void DrawCube(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw);
void DrawTextSimple(IDirect3DDevice9* dev, float x, float y, D3DCOLOR color, const char* text);
void DrawSanta(LPDIRECT3DDEVICE9 dev, const D3DXVECTOR3& pos, float size, float yaw);

void updateViewMatrix(IDirect3DDevice9* dev);
void updateCameraPositions();
bool ProjectWorldToScreen(IDirect3DDevice9* dev, const D3DXVECTOR3& world, D3DXVECTOR3& out);

void OnDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT StartIndex, UINT PrimCount);

void OnEndScene(IDirect3DDevice9* dev);
D3DXVECTOR3 Vec3ToVector3(Vec3 vec3);