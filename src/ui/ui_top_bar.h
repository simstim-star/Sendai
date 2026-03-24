#pragma once

#include "sendai_nk.h"

typedef struct UI_Renderer UI_Renderer;
typedef struct S_Scene S_Scene;
typedef struct IDXGIAdapter3 IDXGIAdapter3;

typedef struct UI_TopBarState {
	IDXGIAdapter3 *Adapter;
	BOOL ShowInfo;
	enum E_TopBarTab { ETBS_FILE_TAB, ETBS_INFO_TAB } ActiveTab;
} UI_TopBarState;

UI_EAction UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State);