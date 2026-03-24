#include "../core/pch.h"

#include "ui.h"

#include "../core/engine.h"
#include "../core/log.h"
#include "../dx_helpers/desc_helpers.h"
#include "../renderer/renderer.h"
#include "../renderer/texture.h"
#include "../win32/file_dialog.h"
#include "../win32/win_path.h"

struct nk_image UI_TEXTURES[UI_EUT_NUM_USER_TEXTURES];

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void LoadCustomTextures(R_Core *Renderer);

/****************************************************
	Public functions
*****************************************************/

void
UI_Init(S_UI *const UI, R_Core *Renderer)
{
	UI->Renderer.Context = nk_d3d12_init(Renderer->Device, Renderer->Width, Renderer->Height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER, UI_EUT_NUM_USER_TEXTURES);
	UI->Renderer.Width = Renderer->Width;
	UI->Renderer.Height = Renderer->Height;

	UI->Action[UI_ACTION_NONE] = S_DoNothing;
	UI->Action[UI_ACTION_FILE_OPEN] = S_FileOpen;
	UI->Action[UI_ACTION_WIREFRAME_BUTTON_CLICKED] = S_WireframeMode;

	struct nk_font_atlas *Atlas;
	nk_d3d12_font_stash_begin(&Atlas);
	nk_d3d12_font_stash_end(Renderer->CommandList);
	LoadCustomTextures(Renderer);
}

void (*UI_GetAction(S_UI *const UI))(Sendai *const Engine)
{
	UI_EAction Action = UI_DrawTopBar(&UI->Renderer, &UI->State.TopBar) | UI_DrawToolbar(&UI->Renderer, &UI->State.ToolBar) |
						UI_DrawBottomPanel(&UI->Renderer, &UI->State.BottomBar);

	return UI->Action[Action];
}

UI_EAction
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

void
UI_SetTextureInNkHeap(UINT nkSrvIndex, ID3D12Resource *Texture)
{
	if (nkSrvIndex >= UI_EUT_NUM_USER_TEXTURES) {
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

/****************************************************
	Implementation of private functions
*****************************************************/

void
LoadCustomTextures(R_Core *Renderer)
{
	WCHAR WireframePath[512];
	Win32FullPath(L"/assets/images/wireframe.png", WireframePath, _countof(WireframePath));
	R_CreateUITexture(WireframePath, Renderer, UI_EUT_WIREFRAME);
	WCHAR CameraPath[512];
	Win32FullPath(L"/assets/images/camera.png", CameraPath, _countof(CameraPath));
	R_CreateUITexture(CameraPath, Renderer, UI_EUT_CAMERA);
}
