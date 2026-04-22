#pragma once
#include "render_types.h"

#define IS_LIGHT_ACTIVE(mask, i) (((mask) >> (i)) & 1)

typedef struct S_Scene S_Scene;
typedef struct R_Camera R_Camera;
typedef struct R_Core R_Core;

void R_LightsInit(S_Scene *const Scene, const R_Camera *const Camera);
void R_UpdateLights(BYTE ActiveLightMask, const R_Light *const InLights, R_Light *const OutLights, UINT NumLights);
void R_DrawLightBillboards(R_Core *const Renderer, R_Light *Lights, BYTE ActiveLightMask, R_MeshConstants *const MeshConstants);
