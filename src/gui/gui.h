#pragma once

#include "../renderer/render_types.h"
#include "windows.h"

typedef struct Sendai_Renderer Sendai_Renderer;

typedef struct SendaiGui_Context {
	struct nk_context *ctx;
} SendaiGui_Context;

void SendaiGui_init(SendaiGui_Context *const gui, int width, int height, struct ID3D12Device *device, struct ID3D12GraphicsCommandList *command_list);

void SendaiGui_input_begin(const SendaiGui_Context *gui);

void SendaiGui_input_end(const SendaiGui_Context *gui);

void SendaiGui_log_window(SendaiGui_Context *const gui);

void SendaiGui_draw_top_bar(SendaiGui_Context *gui, const char **curr_window);

void SendaiGui_draw(struct ID3D12GraphicsCommandList *command_list);

void SendaiGui_resize(const int width, const int height);

int SendaiGui_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);

void SendaiGui_destroy();