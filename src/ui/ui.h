#pragma once

#include "sendai_nk.h"

#include "ui_bottom_panel.h"
#include "ui_tool_bar.h"
#include "ui_top_bar.h"

typedef struct nk_context nk_context;
typedef struct S_Scene S_Scene;
typedef struct R_Camera R_Camera;
typedef struct Sendai Sendai;
typedef struct ID3D12GraphicsCommandList ID3D12GraphicsCommandList;
typedef struct ID3D12Resource ID3D12Resource;

extern struct nk_image UI_TEXTURES[UI_EUT_NUM_USER_TEXTURES];

typedef struct UI_Renderer {
	nk_context *Context;
	UINT Width;
	UINT Height;
} UI_Renderer;

typedef struct UI_State {
	UI_TopBarState TopBar;
	UI_BottomPanelState BottomBar;
	UI_ToolBarState ToolBar;
} UI_State;

typedef struct S_UI {
	UI_Renderer Renderer;
	UI_State State;
	VOID (*Action[N_UI_ACTIONS])(Sendai *const Engine);
} S_UI;

VOID UI_Init(S_UI *const UI, struct R_Core *Renderer);

VOID (*UI_GetAction(S_UI *const UI))(Sendai *const Engine);

VOID UI_InputBegin(const UI_Renderer *UI);

VOID UI_InputEnd(const UI_Renderer *UI);

UI_EAction UI_LogWindow(UI_Renderer *const UI);

VOID UI_Draw(ID3D12GraphicsCommandList *CommandList);

VOID UI_Resize(UI_Renderer *UI, const INT Width, const INT Height);

INT UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

VOID UI_Destroy();

VOID UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *OutTexture);
