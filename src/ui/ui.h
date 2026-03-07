#pragma once

#include "../renderer/render_types.h"
#include "windows.h"

typedef struct R_World R_World;
typedef struct Sendai Sendai;

typedef struct UI_Renderer {
	struct nk_context *Context;
	UINT WindowWidth;
} UI_Renderer;

void UI_Init(UI_Renderer *const UI, int Width, int Height, struct ID3D12Device *Device, struct ID3D12GraphicsCommandList *CommandList);

void UI_InputBegin(const UI_Renderer *UI);

void UI_InputEnd(const UI_Renderer *UI);

void UI_LogWindow(UI_Renderer *const UI);

void UI_DrawTopBar(UI_Renderer *UI, Sendai *Engine);

void UI_Draw(struct ID3D12GraphicsCommandList *CommandList);

void UI_Resize(UI_Renderer *UI, const int Width, const int Height);

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

void UI_Destroy();