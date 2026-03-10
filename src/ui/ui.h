#pragma once

#include "../renderer/render_types.h"
#include "windows.h"


typedef enum { UI_ACTION_NONE = 0, UI_ACTION_FILE_OPEN, UI_ACTION_EDIT_UNDO, UI_ACTION_HELP_ABOUT } UI_Action;

typedef struct UI_Renderer {
	struct nk_context *Context;
	UINT Width;
	UINT Height;

	int ShowLog;
	float BottomBarHeight;
	BOOL bIsDraggingBottom;
} UI_Renderer;

void UI_Init(UI_Renderer *const UI, int Width, int Height, struct ID3D12Device *Device, struct ID3D12GraphicsCommandList *CommandList);

void UI_InputBegin(const UI_Renderer *UI);

void UI_InputEnd(const UI_Renderer *UI);

UI_Action UI_LogWindow(UI_Renderer *const UI);

UI_Action UI_DrawTopBar(UI_Renderer *UI);

UI_Action UI_DrawBottomBar(UI_Renderer *UI);

void UI_Draw(struct ID3D12GraphicsCommandList *CommandList);

void UI_Resize(UI_Renderer *UI, const int Width, const int Height);

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

void UI_Destroy();