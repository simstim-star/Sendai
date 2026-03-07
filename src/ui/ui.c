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

#include <d3d12.h>
#include <stdio.h>


#include "ui.h"
#include "../win32/file_dialog.h"
#include "../../deps/nuklear.h"
#include "../core/log.h"
#include "../shaders/nuklear/nuklear_d3d12.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static struct nk_colorf ColorToNuklear(R_Color *color);

/****************************************************
	Public functions
*****************************************************/

void UI_Init(UI_Renderer *const UI, int Width, int Height, ID3D12Device *Device, ID3D12GraphicsCommandList *CommandList)
{
	UI->Context = nk_d3d12_init(Device, Width, Height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER, USER_TEXTURES);
	UI->Width = Width;
	{
		struct nk_font_atlas *Atlas;
		nk_d3d12_font_stash_begin(&Atlas);
		nk_d3d12_font_stash_end(CommandList);
	}
}

UI_Action UI_DrawTopBar(UI_Renderer *UI)
{
	const float BarHeight = 35.0f;
	UI_Action Action = UI_ACTION_NONE;

	if (nk_begin(UI->Context, "TopBar", nk_rect(0, 0, UI->Width, BarHeight), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_dynamic(UI->Context, BarHeight - 5, 4);
		if (nk_button_label(UI->Context, "File")) {
			Action =  UI_ACTION_FILE_OPEN;
		}
	}
	nk_end(UI->Context);
	return Action;
}

UI_Action UI_LogWindow(UI_Renderer *const UI)
{
	struct nk_context *Context = UI->Context;
	const float WindowX = 900.0f;
	const float WindowY = 50.0f;
	const float WindowW = 600.0f;
	const float WindowH = 700.0f;
	const nk_flags WindowFlags = NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;
	if (nk_begin(Context, "System Log", nk_rect(WindowX, WindowY, WindowW, WindowH), WindowFlags)) {
		nk_layout_row_dynamic(Context, WindowW * 2, 1);
		const nk_flags LogFlags = NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE;
		nk_edit_string(Context, LogFlags, SENDAI_LOG.Buffer, &SENDAI_LOG.Len, SENDAI_LOG.Max, nk_filter_default);
	}
	nk_end(Context);

	return UI_ACTION_NONE;
}

void UI_InputBegin(const UI_Renderer *UI)
{
	nk_input_begin(UI->Context);
}

void UI_InputEnd(const UI_Renderer *UI)
{
	nk_input_end(UI->Context);
}

void UI_Draw(ID3D12GraphicsCommandList *CommandList)
{
	nk_d3d12_render(CommandList, NK_ANTI_ALIASING_ON);
}

void UI_Resize(UI_Renderer *UI, const int Width, const int Height)
{
	UI->Width = Width;
	nk_d3d12_resize(Width, Height);
}

int UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	return nk_d3d12_handle_event(hWnd, Message, wParam, lParam);
}

void UI_Destroy()
{
	nk_d3d12_shutdown();
}

static struct nk_colorf ColorToNuklear(R_Color *Color)
{
	return (struct nk_colorf){
	  .r = Color->R,
	  .g = Color->G,
	  .b = Color->B,
	  .a = Color->A,
	};
}
