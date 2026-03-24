#pragma once

typedef struct R_MeshConstants R_MeshConstants;
typedef struct R_Core R_Core;
typedef struct R_Camera R_Camera;

void R_RenderGrid(R_MeshConstants *const MeshConstants, R_Core *const Renderer, const float HalfSide);