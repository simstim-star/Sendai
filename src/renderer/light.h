#pragma once
#include "render_types.h"

#define NUM_LIGHTS 7
#define IS_LIGHT_ACTIVE(mask, i) (((mask) >> (i)) & 1)

typedef struct S_Scene S_Scene;
typedef struct R_Camera R_Camera;
typedef struct R_Core R_Core;

void R_LightsInit(S_Scene *Scene, R_Camera *Camera);
void R_UpdateLights(BYTE ActiveLightMask, R_Light *InLights, R_Light *OutLights, UINT NumLights);
void R_RenderLightBillboard(R_MeshConstants *MeshConstants, R_Core *const Renderer, XMFLOAT3 Tint, UINT SrvIndex);