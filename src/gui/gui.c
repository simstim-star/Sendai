#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define USER_TEXTURES 6

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D12_IMPLEMENTATION

#include <stdio.h>
#include <d3d12.h>

#include "gui.h"

#include "../../deps/nuklear/nuklear.h"
#include "../shaders/nuklear/nuklear_d3d12.h"

static struct nk_colorf color_to_nk(snr_color_t *color);

void sng_init(sng_gui_t *const gui, int width, int height, ID3D12Device *device, ID3D12GraphicsCommandList *command_list) {
	gui->ctx = nk_d3d12_init(device, width, height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER, USER_TEXTURES);
	{
		struct nk_font_atlas *atlas;
		nk_d3d12_font_stash_begin(&atlas);
		nk_d3d12_font_stash_end(command_list);
	}
}

void sng_update_triangle_menu(sng_gui_t *const gui, snr_vertex_t *const triangle_data) {
	if (nk_begin(gui->ctx, "Triangle", nk_rect(50, 50, 230, 250),
				 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
		for (int i = 0; i < 3; ++i) {
			char label[8];
			snprintf(label, sizeof(label), "v%d:", i + 1);

			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label(gui->ctx, label, NK_TEXT_LEFT);
			nk_layout_row_dynamic(gui->ctx, 25, 1);

			struct nk_colorf v_color = color_to_nk(&triangle_data[i].color);

			if (nk_combo_begin_color(gui->ctx, nk_rgb_cf(v_color), nk_vec2(nk_widget_width(gui->ctx), 400))) {
				nk_layout_row_dynamic(gui->ctx, 120, 1);
				v_color = nk_color_picker(gui->ctx, v_color, NK_RGBA);
				nk_layout_row_dynamic(gui->ctx, 25, 1);

				triangle_data[i].color.x = nk_propertyf(gui->ctx, "#R:", 0, v_color.r, 1.0f, 0.01f, 0.005f);
				triangle_data[i].color.y = nk_propertyf(gui->ctx, "#G:", 0, v_color.g, 1.0f, 0.01f, 0.005f);
				triangle_data[i].color.z = nk_propertyf(gui->ctx, "#B:", 0, v_color.b, 1.0f, 0.01f, 0.005f);
				triangle_data[i].color.w = nk_propertyf(gui->ctx, "#A:", 0, v_color.a, 1.0f, 0.01f, 0.005f);

				nk_combo_end(gui->ctx);
			}
		}
	}
	nk_end(gui->ctx);
}


void sng_input_begin(const sng_gui_t *gui) {
	nk_input_begin(gui->ctx);
}

void sng_input_end(const sng_gui_t *gui) {
	nk_input_end(gui->ctx);
}

void sng_draw(ID3D12GraphicsCommandList *command_list) {
	nk_d3d12_render(command_list, NK_ANTI_ALIASING_ON);
}

void sng_resize(const int width, const int height) {
	nk_d3d12_resize(width, height);
}

int sng_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	return nk_d3d12_handle_event(wnd, msg, wparam, lparam);
}

void sng_destroy() {
	nk_d3d12_shutdown();
}

static struct nk_colorf color_to_nk(snr_color_t *color) {
	return (struct nk_colorf){
		.r = color->r,
		.g = color->g,
		.b = color->b,
		.a = color->a,
	};
}
