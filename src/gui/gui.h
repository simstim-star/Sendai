#pragma once

#include "../renderer/render_types.h"
#include "windows.h"

typedef struct SR_renderer_t SR_renderer_t;

typedef struct SGUI_context_t {
	struct nk_context *ctx;
} SGUI_context_t;



void SGUI_init(SGUI_context_t *const gui, int width, int height, struct ID3D12Device *device, struct ID3D12GraphicsCommandList *command_list);

void SGUI_input_begin(const SGUI_context_t *gui);

void SGUI_input_end(const SGUI_context_t *gui);

void SGUI_draw_top_bar(SGUI_context_t *gui, const char **curr_window);

void SGUI_update_triangle_menu(SGUI_context_t *const gui, snr_vertex_t *const triangle_data);

void SGUI_draw(struct ID3D12GraphicsCommandList *command_list);

void SGUI_resize(const int width, const int height);

int SGUI_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);

void SGUI_destroy();