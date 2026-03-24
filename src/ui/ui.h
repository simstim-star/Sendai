#pragma once

#include "sendai_nk.h"

#include "ui_bottom_panel.h"
#include "ui_top_bar.h"
#include "ui_tool_bar.h"

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

void UI_Init(UI_Renderer *const UI, struct R_Core *Renderer);

void UI_Update(Sendai *Engine);

void UI_InputBegin(const UI_Renderer *UI);

void UI_InputEnd(const UI_Renderer *UI);

UI_EAction UI_LogWindow(UI_Renderer *const UI);

void UI_Draw(ID3D12GraphicsCommandList *CommandList);

void UI_Resize(UI_Renderer *UI, const int Width, const int Height);

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

void UI_Destroy();

void UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *OutTexture);
