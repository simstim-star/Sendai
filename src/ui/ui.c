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
	UI->Height = Height;
	{
		struct nk_font_atlas *Atlas;
		nk_d3d12_font_stash_begin(&Atlas);
		nk_d3d12_font_stash_end(CommandList);
	}
}

UI_Action UI_DrawTopBar(UI_Renderer *UI)
{
	const float BarHeight = UI->Height * 0.05f;
	UI_Action Action = UI_ACTION_NONE;

	if (nk_begin(UI->Context, "TopBar", nk_rect(0, 0, UI->Width, BarHeight), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_dynamic(UI->Context, BarHeight * 0.8f, 3);
		if (nk_button_label(UI->Context, "File")) {
			Action =  UI_ACTION_FILE_OPEN;
		}
		if (nk_button_label(UI->Context, "Logs")) {
			UI->ShowLog = !UI->ShowLog;
		}
	}
	nk_end(UI->Context);
	
	if (UI->ShowLog) {
		Action = UI_LogWindow(UI);
	}

	return Action;
}

UI_Action UI_DrawBottomBar(UI_Renderer *UI)
{
	struct nk_context *Ctx = UI->Context;
	const float HandleHeight = 10.0f;

	UI->BottomBarHeight = fmax(UI->BottomBarHeight, UI->Height * 0.08f);
	UI->BottomBarHeight = fmin(UI->BottomBarHeight, UI->Height * 0.9f);

	struct nk_rect BarRect = nk_rect(0, UI->Height - UI->BottomBarHeight, UI->Width, UI->BottomBarHeight);

	if (nk_begin(Ctx, "BottomBar", BarRect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_static(Ctx, HandleHeight, UI->Width, 1);
		struct nk_rect Bounds = nk_widget_bounds(Ctx);
		struct nk_command_buffer *Canvas = nk_window_get_canvas(Ctx);
		
		BOOL bIsMouseDown = nk_input_is_mouse_down(&Ctx->input, NK_BUTTON_LEFT);
		BOOL bIsHoveringHandle = nk_input_is_mouse_hovering_rect(&Ctx->input, Bounds);
		UI->bIsDraggingBottom = bIsMouseDown && (UI->bIsDraggingBottom || bIsHoveringHandle);
		
		if (UI->bIsDraggingBottom) {
			UI->BottomBarHeight -= Ctx->input.mouse.delta.y;
			nk_fill_rect(Canvas, Bounds, 0, nk_rgba(150, 150, 255, 255));
		} else if (bIsHoveringHandle) {
			nk_fill_rect(Canvas, Bounds, 0, nk_rgba(100, 100, 120, 200));
		} else {
			struct nk_rect line = nk_rect(Bounds.x + Bounds.w / 2 - 20, Bounds.y + 2, 40, 2);
			nk_fill_rect(Canvas, line, 1.0f, nk_rgba(60, 60, 60, 255));
		}

		const float ItemWidth = UI->Width * 0.1f;
		const float BackButtonWidth = 40.0f;

		if (UI->ShowLog) {
			nk_layout_row_begin(Ctx, NK_STATIC, 25, 2);
			nk_layout_row_push(Ctx, BackButtonWidth);
			if (nk_button_label(Ctx, "<")) {
				UI->ShowLog = 0;
			}
			nk_layout_row_push(Ctx, 100);
			nk_label(Ctx, "  System Log", NK_TEXT_LEFT);
			nk_layout_row_end(Ctx);

			float LogAreaHeight = UI->BottomBarHeight - (HandleHeight + 40.0f);
			nk_layout_row_dynamic(Ctx, fmax(10.0f, LogAreaHeight), 1);

			const nk_flags LogFlags = NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE;
			nk_edit_string(Ctx, LogFlags, SENDAI_LOG.Buffer, &SENDAI_LOG.Len, SENDAI_LOG.Max, nk_filter_default);
		} else {
			nk_layout_row_static(Ctx, 30, ItemWidth, 2);
			if (nk_button_label(Ctx, "Show Log")) {
				UI->ShowLog = 1;
			}
		}
	}
	nk_end(Ctx);

	return UI_ACTION_NONE;
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
	UI->Height = Height;
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
