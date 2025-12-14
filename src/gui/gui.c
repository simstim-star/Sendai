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
#include "../core/log.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static struct nk_colorf color_to_nk(Sendai_Color *color);

/****************************************************
	Public functions
*****************************************************/

void SendaiGui_init(SendaiGui_Context *const gui, int width, int height, ID3D12Device *device, ID3D12GraphicsCommandList *command_list) {
	gui->ctx = nk_d3d12_init(device, width, height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER, USER_TEXTURES);
	
	{
		struct nk_font_atlas *atlas;
		nk_d3d12_font_stash_begin(&atlas);
		nk_d3d12_font_stash_end(command_list);
	}
}

void SendaiGui_draw_top_bar(SendaiGui_Context *gui, const char **curr_window) {
	const float bar_height = 35.0f;

	if (nk_begin(gui->ctx, "TopBar", nk_rect(0, 0, 800, bar_height), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_dynamic(gui->ctx, bar_height - 5, 4);

		if (nk_button_label(gui->ctx, "Home"))
			*curr_window = "home";
		else if (nk_button_label(gui->ctx, "Triangle"))
			*curr_window = "triangle";
	}
	nk_end(gui->ctx);
}

void SendaiGui_log_window(SendaiGui_Context *const gui) {
	struct nk_context *ctx = gui->ctx;
	const float window_x = 900.0f;
	const float window_y = 50.0f;
	const float window_w = 600.0f;
	const float window_h = 700.0f;
	const nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;
	if (nk_begin(ctx, "System Log", nk_rect(window_x, window_y, window_w, window_h), window_flags)) {
		nk_layout_row_dynamic(ctx, window_w * 2, 1);
		const nk_flags log_flags = NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE;
		nk_edit_string(ctx, log_flags, SENDAI_LOG.buffer, &SENDAI_LOG.len, SENDAI_LOG.max, nk_filter_default);
	}
	nk_end(ctx);
}

void SendaiGui_input_begin(const SendaiGui_Context *gui) {
	nk_input_begin(gui->ctx);
}

void SendaiGui_input_end(const SendaiGui_Context *gui) {
	nk_input_end(gui->ctx);
}

void SendaiGui_draw(ID3D12GraphicsCommandList *command_list) {
	nk_d3d12_render(command_list, NK_ANTI_ALIASING_ON);
}

void SendaiGui_resize(const int width, const int height) {
	nk_d3d12_resize(width, height);
}

int SendaiGui_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	return nk_d3d12_handle_event(wnd, msg, wparam, lparam);
}

void SendaiGui_destroy() {
	nk_d3d12_shutdown();
}

static struct nk_colorf color_to_nk(Sendai_Color *color) {
	return (struct nk_colorf){
		.r = color->r,
		.g = color->g,
		.b = color->b,
		.a = color->a,
	};
}
