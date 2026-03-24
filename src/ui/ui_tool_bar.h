#pragma once

#include "sendai_nk.h"

typedef struct UI_Renderer UI_Renderer;
typedef struct R_Camera R_Camera;

typedef struct UI_ToolBarState {
	BOOL Wireframe;
	BOOL Grid;
	R_Camera *Camera;
} UI_ToolBarState;

UI_EAction UI_DrawToolbar(UI_Renderer *UI, UI_ToolBarState *State);