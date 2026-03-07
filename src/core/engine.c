#include "engine.h"
#include "../assets/gltf.h"
#include "../ui/ui.h"
#include "../win32/file_dialog.h"
#include "../renderer/renderer.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void InitWindow(Sendai *Engine);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
static void EngineUpdate(Sendai *Engine);
static void EngineDraw(Sendai *Engine);
static void GuiUpdate(Sendai *Engine);

/****************************************************
	Public functions
*****************************************************/
int Sendai_run()
{
	Sendai Engine = {
	  .Title = L"Sendai",
	  .WorldRenderer = {.Width = 1280, .Height = 720},
	  .Camera = R_CameraSpawn((XMFLOAT3){0, 0, -10}),
	  .Scene.SceneArena = S_ArenaInit(GIGABYTES(2)),
	  .bRunning = true
	};

	Engine.Camera.Yaw = 2 * XM_PI;

	InitWindow(&Engine);
	R_Init(&Engine.WorldRenderer, Engine.hWnd);

	CreateSceneRootSig(Engine.WorldRenderer.Device, &Engine.Scene.RootSign);
	PCWSTR shaders_path = wcscat(Engine.WorldRenderer.AssetsPath, L"src/shaders/gltf/gltf.hlsl");
	CompileSceneVS(shaders_path, &Engine.Scene.VS);
	CompileScenePS(shaders_path, &Engine.Scene.PS);
	CreateScenePipelineState(&Engine.WorldRenderer, &Engine.Scene);

	UI_Init(&Engine.UI, Engine.WorldRenderer.Width, Engine.WorldRenderer.Height, Engine.WorldRenderer.Device, Engine.WorldRenderer.CommandList);
	S_TimerInit(&Engine.Timer);

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		UI_InputBegin(&Engine.UI);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UI_InputEnd(&Engine.UI);
		if (Engine.bRunning) {
			EngineUpdate(&Engine);
			EngineDraw(&Engine);
		}
	}

	R_Destroy(&Engine.WorldRenderer);
	S_ArenaRelease(&Engine.Scene.SceneArena);
	return (int)(msg.wParam);
}

/****************************************************
	Implementation of private functions
*****************************************************/

void InitWindow(Sendai *engine)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = engine->hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
	wc.lpszClassName = L"SendaiClass";
	RegisterClassEx(&wc);
	RECT rect = {0, 0, (LONG)(engine->WorldRenderer.Width), (LONG)(engine->WorldRenderer.Height)};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	engine->hWnd = CreateWindow(wc.lpszClassName, engine->Title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 
		rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,engine->hInstance, engine);
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (UI_HandleEvent(hwnd, message, wparam, lparam))
		return 0;

	Sendai *engine = (Sendai *)(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message) {
	case WM_CREATE: {
		LPCREATESTRUCT p_create_struct = (LPCREATESTRUCT)(lparam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(p_create_struct->lpCreateParams));
		return 0;
	}
	case WM_SIZE:
		if (engine && engine->WorldRenderer.SwapChain) {
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			R_SwapchainResize(&engine->WorldRenderer, width, height);
			UI_Resize(&engine->UI, width, height);
		}
		return 0;

	case WM_PAINT:
		ValidateRect(hwnd, NULL);
		return 0;

	case WM_KEYDOWN:
		if (engine) {
			R_CameraOnKeyDown(&engine->Camera, wparam);
		}
		return 0;

	case WM_KEYUP:
		if (engine) {
			R_CameraOnKeyUp(&engine->Camera, wparam);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}
static void EngineUpdate(Sendai *engine) {
    S_Tick(&engine->Timer);
    R_CameraUpdate(&engine->Camera, TicksToSeconds_FLOAT(engine->Timer.ElapsedTicks));
    GuiUpdate(engine);
}

static void EngineDraw(Sendai *engine) {
    R_Update(&engine->WorldRenderer, &engine->Camera, &engine->Scene);
    R_Draw(&engine->WorldRenderer, &engine->Scene);
}

void GuiUpdate(Sendai *engine)
{
	UI_DrawTopBar(&engine->UI, engine);
	UI_LogWindow(&engine->UI);
}