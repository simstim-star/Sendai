#pragma once

#include "../renderer/render_types.h"

typedef enum { UI_ACTION_NONE = 0, UI_ACTION_FILE_OPEN, UI_ACTION_WIREFRAME_BUTTON_CLICKED } UI_Action;

typedef enum { TEXTURE_WIREFRAME, NUM_USER_TEXTURES } USER_TEXTURES;

typedef struct UI_Renderer {
	struct nk_context *Context;
	UINT Width;
	UINT Height;

	ID3D12Resource *IconTextureResource;
} UI_Renderer;


typedef struct UI_BottomBarState {
	float BottomBarHeight;
	BOOL bIsDraggingBottom;
	UINT32 FPS;
	UINT FrameCounter;
} UI_BottomBarState;

typedef struct UI_TopBarState {
	BOOL ShowLog;
} UI_TopBarState;

typedef struct UI_ToolBarState {
	BOOL Wireframe;
} UI_ToolBarState;

typedef struct UI_State {
	UI_TopBarState TopBar;
	UI_BottomBarState BottomBar;
	UI_ToolBarState ToolBar;
} UI_State;

void UI_Init(UI_Renderer *const UI, int Width, int Height, struct ID3D12Device *Device, struct ID3D12GraphicsCommandList *CommandList);

void UI_InputBegin(const UI_Renderer *UI);

void UI_InputEnd(const UI_Renderer *UI);

UI_Action UI_LogWindow(UI_Renderer *const UI);

UI_Action UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State);

UI_Action UI_DrawToolbarButton(UI_Renderer *UI, UI_ToolBarState *State);

UI_Action UI_DrawBottomBar(UI_Renderer *UI, UI_BottomBarState *State);

void UI_Draw(struct ID3D12GraphicsCommandList *CommandList);

void UI_Resize(UI_Renderer *UI, const int Width, const int Height);

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

void UI_Destroy();

void UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *OutTexture);
