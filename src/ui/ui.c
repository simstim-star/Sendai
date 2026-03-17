#include "../core/pch.h"

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
#include "../../deps/nuklear.h"
#include "../shaders/nuklear/nuklear_d3d12.h"

#include "ui.h"
#include "../core/log.h"
#include "../win32/file_dialog.h"
#include "../renderer/renderer.h"

#include "../dx_helpers/desc_helpers.h"

static struct nk_image UI_TEXTURES[NUM_USER_TEXTURES];

/****************************************************
	Forward declaration of private functions
*****************************************************/

static struct nk_colorf ColorToNuklear(XMFLOAT4 *color);

/****************************************************
	Public functions
*****************************************************/

void
UI_Init(UI_Renderer *const UI, int Width, int Height, ID3D12Device *Device, ID3D12GraphicsCommandList *CommandList)
{
	UI->Context = nk_d3d12_init(Device, Width, Height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER, NUM_USER_TEXTURES);
	UI->Width = Width;
	UI->Height = Height;

	struct nk_font_atlas *Atlas;
	nk_d3d12_font_stash_begin(&Atlas);
	nk_d3d12_font_stash_end(CommandList);
}

UI_Action
UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State)
{
	const float BarHeight = UI->Height * 0.05f;
	UI_Action Action = UI_ACTION_NONE;

	if (nk_begin(UI->Context, "TopBar", nk_rect(0, 0, UI->Width, BarHeight), NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(UI->Context, BarHeight * 0.8f, 3);
		if (nk_button_label(UI->Context, "File")) {
			Action = UI_ACTION_FILE_OPEN;
		}
		if (nk_button_label(UI->Context, "Logs")) {
			State->ShowLog = !State->ShowLog;
			Action = UI_ACTION_NONE;
		}
	}
	nk_end(UI->Context);

	if (State->ShowLog) {
		UI_LogWindow(UI);
	}

	return Action;
}

UI_Action
UI_DrawToolbarButton(UI_Renderer *UI, UI_ToolBarState *State)
{
	struct nk_context *Ctx = UI->Context;
	UI_Action Action = UI_ACTION_NONE;

	const float TopBarHeight = UI->Height * 0.05f;
	const float BtnSize = 50.0f;
	struct nk_rect WindowRect = nk_rect(10, TopBarHeight + 10, BtnSize, BtnSize);
	nk_style_push_vec2(Ctx, &Ctx->style.window.padding, nk_vec2(0, 0));
	if (nk_begin(Ctx, "IconButton", WindowRect, NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_static(Ctx, BtnSize, BtnSize, 1);
		if (nk_button_image(Ctx, UI_TEXTURES[TEXTURE_WIREFRAME])) {
			Action = UI_ACTION_WIREFRAME_BUTTON_CLICKED;
			State->Wireframe = !State->Wireframe;
		}
	}
	nk_end(Ctx);
	nk_style_pop_vec2(Ctx);

	return Action;
}

UI_Action
UI_DrawBottomBar(UI_Renderer *UI, UI_BottomBarState *State)
{
	struct nk_context *Ctx = UI->Context;
	const float HandleHeight = 10.0f;
	const float InfoBarHeight = 22.0f;

	State->BottomBarHeight = fmax(State->BottomBarHeight, UI->Height * 0.08f);
	State->BottomBarHeight = fmin(State->BottomBarHeight, UI->Height * 0.9f);

	float MaxAvailableHeight = UI->Height - InfoBarHeight;
	struct nk_rect BarRect = nk_rect(0, MaxAvailableHeight - State->BottomBarHeight, UI->Width, State->BottomBarHeight);

	if (nk_begin(Ctx, "BottomBar", BarRect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_static(Ctx, HandleHeight, UI->Width, 1);
		struct nk_rect Bounds = nk_widget_bounds(Ctx);
		struct nk_command_buffer *Canvas = nk_window_get_canvas(Ctx);

		BOOL bIsMouseDown = nk_input_is_mouse_down(&Ctx->input, NK_BUTTON_LEFT);
		BOOL bIsHoveringHandle = nk_input_is_mouse_hovering_rect(&Ctx->input, Bounds);
		State->bIsDraggingBottom = bIsMouseDown && (State->bIsDraggingBottom || bIsHoveringHandle);

		if (State->bIsDraggingBottom) {
			State->BottomBarHeight -= Ctx->input.mouse.delta.y;
			nk_fill_rect(Canvas, Bounds, 0, nk_rgba(150, 150, 255, 255));
		} else if (bIsHoveringHandle) {
			nk_fill_rect(Canvas, Bounds, 0, nk_rgba(100, 100, 120, 200));
		} else {
			struct nk_rect line = nk_rect(Bounds.x + Bounds.w / 2 - 20, Bounds.y + 2, 40, 2);
			nk_fill_rect(Canvas, line, 1.0f, nk_rgba(60, 60, 60, 255));
		}

		// Just to use as reference in the future
		//const float ItemWidth = UI->Width * 0.1f;
		//const float BackButtonWidth = 40.0f;
		//if (UI->ShowLog) {
		//	nk_layout_row_begin(Ctx, NK_STATIC, 25, 2);
		//	nk_layout_row_push(Ctx, BackButtonWidth);
		//	if (nk_button_label(Ctx, "<")) {
		//		UI->ShowLog = 0;
		//	}
		//	nk_layout_row_push(Ctx, 100);
		//	nk_label(Ctx, "  System Log", NK_TEXT_LEFT);
		//	nk_layout_row_end(Ctx);
		//
		//	float LogAreaHeight = State->BottomBarHeight - (HandleHeight + 40.0f);
		//	nk_layout_row_dynamic(Ctx, fmax(10.0f, LogAreaHeight), 1);
		//
		//	const nk_flags LogFlags = NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE;
		//	nk_edit_string(Ctx, LogFlags, SENDAI_LOG.Buffer, &SENDAI_LOG.Len, SENDAI_LOG.Max, nk_filter_default);
		//} else {
		//	nk_layout_row_static(Ctx, 30, ItemWidth, 2);
		//	if (nk_button_label(Ctx, "Show Log")) {
		//		UI->ShowLog = 1;
		//	}
		//}
	}
	nk_end(Ctx);

	struct nk_color InfoBarColor = nk_rgba(30, 30, 35, 255);
	nk_style_push_style_item(Ctx, &Ctx->style.window.fixed_background, nk_style_item_color(InfoBarColor));
	nk_style_push_vec2(Ctx, &Ctx->style.window.padding, nk_vec2(4, 2));
	nk_style_push_color(Ctx, &Ctx->style.text.color, nk_rgba(0, 255, 0, 255));
	struct nk_rect InfoRect = nk_rect(0, UI->Height - InfoBarHeight, UI->Width, InfoBarHeight);

	if (nk_begin(Ctx, "InfoBar", InfoRect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_begin(Ctx, NK_DYNAMIC, InfoBarHeight - 4, 2);
		nk_layout_row_push(Ctx, 0.85f);
		nk_label(Ctx, " Sendai Engine v0.1", NK_TEXT_LEFT);

		nk_layout_row_push(Ctx, 0.15f);
		char FPSBuffer[16];
		snprintf(FPSBuffer, sizeof(FPSBuffer), "%u FPS", State->FPS);
		nk_label(Ctx, FPSBuffer, NK_TEXT_RIGHT);
		nk_layout_row_end(Ctx);
	}
	nk_end(Ctx);

	nk_style_pop_color(Ctx);	  
	nk_style_pop_vec2(Ctx);		  
	nk_style_pop_style_item(Ctx); 

	return UI_ACTION_NONE;
}

UI_Action
UI_LogWindow(UI_Renderer *const UI)
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
		
		int RequiredBytes =
			WideCharToMultiByte(CP_UTF8, 0, SENDAI_LOG.Buffer, SENDAI_LOG.Len, SENDAI_LOG.UTF8Buffer, sizeof(SENDAI_LOG.UTF8Buffer) - 1, NULL, NULL);

		if (RequiredBytes >= 0) {
			SENDAI_LOG.UTF8Buffer[RequiredBytes] = '\0';
			nk_edit_string(Context, LogFlags, SENDAI_LOG.UTF8Buffer, &RequiredBytes, sizeof(SENDAI_LOG.UTF8Buffer), nk_filter_default);
		}
	}
	nk_end(Context);

	return UI_ACTION_NONE;
}

void
UI_InputBegin(const UI_Renderer *UI)
{
	nk_input_begin(UI->Context);
}

void
UI_InputEnd(const UI_Renderer *UI)
{
	nk_input_end(UI->Context);
}

void
UI_Draw(ID3D12GraphicsCommandList *CommandList)
{
	nk_d3d12_render(CommandList, NK_ANTI_ALIASING_ON);
}

void
UI_Resize(UI_Renderer *UI, const int Width, const int Height)
{
	UI->Width = Width;
	UI->Height = Height;
	nk_d3d12_resize(Width, Height);
}

int
UI_HandleEvent(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	return nk_d3d12_handle_event(hWnd, Message, wParam, lParam);
}

void
UI_Destroy()
{
	nk_d3d12_shutdown();
}

static struct nk_colorf
ColorToNuklear(XMFLOAT4 *Color)
{
	return (struct nk_colorf){
	  .r = Color->x,
	  .g = Color->y,
	  .b = Color->z,
	  .a = Color->w,
	};
}

void
UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *Texture)
{
	if (nkSrvIndex >= NUM_USER_TEXTURES) {
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
											   .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
											   .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
											   .Texture2D.MipLevels = 1};
	nk_handle Handle;
	nk_d3d12_set_user_texture(nkSrvIndex, Texture, &SrvDesc, &Handle);
	UI_TEXTURES[nkSrvIndex] = nk_image_handle(Handle);
}
