#pragma once

typedef enum { UI_ACTION_NONE = 0, UI_ACTION_FILE_OPEN, UI_ACTION_WIREFRAME_BUTTON_CLICKED } UI_EAction;

typedef enum { UI_EUT_WIREFRAME, UI_EUT_NUM_USER_TEXTURES } UI_EUserTextures;

typedef struct UI_Renderer {
	struct nk_context *Context;
	UINT Width;
	UINT Height;

	ID3D12Resource *IconTextureResource;
} UI_Renderer;


typedef struct UI_BottomBarState {
	FLOAT BottomBarHeight;
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

void UI_Init(UI_Renderer *const UI, struct R_Core *Renderer);

void UI_Update(struct Sendai *Engine);

void UI_InputBegin(const UI_Renderer *UI);

void UI_InputEnd(const UI_Renderer *UI);

UI_EAction UI_LogWindow(UI_Renderer *const UI);

UI_EAction UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State);

UI_EAction UI_DrawToolbarButton(UI_Renderer *UI, UI_ToolBarState *State);

UI_EAction UI_DrawBottomBar(UI_Renderer *UI, UI_BottomBarState *State);

void UI_Draw(struct ID3D12GraphicsCommandList *CommandList);

void UI_Resize(UI_Renderer *UI, const int Width, const int Height);

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

void UI_Destroy();

void UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *OutTexture);
