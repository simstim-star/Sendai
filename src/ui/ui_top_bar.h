#pragma once

#include "sendai_nk.h"

typedef struct UI_Renderer UI_Renderer;
typedef struct S_Scene S_Scene;

typedef struct UI_TopBarState {
	BOOL ShowLog;
	enum E_TopBarTab { ETBS_FILE_TAB, ETBS_LOG_TAB } ActiveTab;
} UI_TopBarState;

UI_EAction UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State);