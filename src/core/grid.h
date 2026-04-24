#pragma once
#include <DirectXMathC.h>

typedef struct R_MeshConstants R_MeshConstants;
typedef struct R_Core R_Core;
typedef struct R_Camera R_Camera;

VOID R_CreateGrid(R_Core *const Renderer, const FLOAT HalfSide);
VOID R_DrawGrid(R_Core *const Renderer, R_MeshConstants *const MeshConstants);