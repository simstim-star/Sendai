#pragma once

#include "sendai_nk.h"

typedef struct UI_Renderer UI_Renderer;
typedef struct S_Scene S_Scene;

typedef struct UI_BottomBarState {
	FLOAT BottomPanelHeight;
	BOOL bIsDraggingBottom;
	UINT32 FPS;
	UINT FrameCounter;
	UINT SelectedModelIndex;
	UINT SelectedLightIndex;
	S_Scene *Scene;
	enum E_BottomBarTab { EBBS_LOG_TAB, EBBS_SCENE_TAB, EBBS_LIGHT_TAB } ActiveTab;
} UI_BottomPanelState;

UI_EAction UI_DrawBottomPanel(UI_Renderer *UI, UI_BottomPanelState *State);