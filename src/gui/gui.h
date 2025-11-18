#pragma once

#include "../renderer/render_types.h"
#include "windows.h"

typedef struct SR_Renderer SR_Renderer;

typedef struct SGUI_Context {
	struct nk_context *ctx;
} SGUI_Context;

void SGUI_init(SGUI_Context *const gui, int width, int height, struct ID3D12Device *device, struct ID3D12GraphicsCommandList *command_list);

void SGUI_input_begin(const SGUI_Context *gui);

void SGUI_input_end(const SGUI_Context *gui);

void SGUI_draw_top_bar(SGUI_Context *gui, const char **curr_window);

void SGUI_update_triangle_menu(SGUI_Context *const gui, SR_Vertex *const triangle_data);

void SGUI_draw(struct ID3D12GraphicsCommandList *command_list);

void SGUI_resize(const int width, const int height);

int SGUI_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);

void SGUI_destroy();