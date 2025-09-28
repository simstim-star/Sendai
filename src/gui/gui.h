#pragma once

#include "../renderer/render_types.h"
#include "windows.h"

typedef struct snr_renderer_t snr_renderer_t;

typedef struct sng_gui_t {
	struct nk_context *ctx;

	/* what will be manipulated through the gui */
	struct snr_color_t triangle_vertex_colors[3];
} sng_gui_t;

void sng_init(sng_gui_t *const gui, int width, int height, struct ID3D12Device *device, struct ID3D12GraphicsCommandList *command_list);

void sng_input_begin(const sng_gui_t *gui);

void sng_input_end(const sng_gui_t *gui);

void sng_update(sng_gui_t *const gui);

void sng_draw(struct ID3D12GraphicsCommandList *command_list);

void sng_resize(const int width, const int height);

int sng_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);

void sng_destroy();