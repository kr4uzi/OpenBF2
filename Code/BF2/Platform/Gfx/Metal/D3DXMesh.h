// [CHOSEN, not recovered] Additional native D3DX9 mesh entry points.
// Separate from the shared subset until its owner folds in these declarations.
#pragma once
#include "../D3DX9Subset.h"

HRESULT WINAPI D3DXCreatePolygon(IDirect3DDevice9*, FLOAT length, UINT sides,
    ID3DXMesh**, ID3DXBuffer** adjacency);
HRESULT WINAPI D3DXCreateTorus(IDirect3DDevice9*, FLOAT innerRadius, FLOAT outerRadius,
    UINT sides, UINT rings, ID3DXMesh**, ID3DXBuffer** adjacency);
HRESULT WINAPI D3DXComputeNormals(ID3DXBaseMesh*, const DWORD* adjacency);
