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
static void HandleWindowsMessage(Sendai *Engine, MSG *Message);
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
	  .Scene.SceneArena = SendaiArena_init(GIGABYTES(2)),
	  .bRunning = true
	};

	Engine.Camera.Yaw = 2 * XM_PI;

	InitWindow(&Engine);
	R_Init(&Engine.WorldRenderer, Engine.hWnd);

	CreateSceneRootSig(Engine.WorldRenderer.Device, &Engine.Scene.RootSign);
	const WCHAR *shaders_path = wcscat(Engine.WorldRenderer.AssetsPath, L"src/shaders/gltf/gltf.hlsl");
	CompileSceneVS(shaders_path, &Engine.Scene.VS);
	CompileScenePS(shaders_path, &Engine.Scene.PS);
	CreateScenePipelineState(&Engine.WorldRenderer, &Engine.Scene);

	UI_Init(&Engine.UI, Engine.WorldRenderer.Width, Engine.WorldRenderer.Height, Engine.WorldRenderer.Device, Engine.WorldRenderer.CommandList);
	SendaiTimer_init(&Engine.Timer);


	PWSTR file = SelectGLTFPath();
	if (file) {
		SendaiGLTF_load(file, &Engine.Scene);
		CoTaskMemFree(file); // TODO improve this
	}

	for (int i = 0; i < Engine.Scene.MeshCount; ++i) {
		R_Vertices(Engine.WorldRenderer.Device, &Engine.Scene.Meshes[i]);
		R_Indices(Engine.WorldRenderer.Device, &Engine.Scene.Meshes[i]);
		// TODO loop through all textures
		R_UploadTexture(
			&Engine.WorldRenderer, &Engine.Scene.Meshes[0].Textures[0], &Engine.WorldRenderer.ModelGpuTexture, &Engine.WorldRenderer.ModelGpuSrv, 0);
	}

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (Engine.bRunning) {
			EngineUpdate(&Engine);
			EngineDraw(&Engine);
		}
	}

	R_Destroy(&Engine.WorldRenderer);
	SendaiArena_release(&Engine.Scene.SceneArena);
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
			UI_Resize(width, height);
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
    Sendai_tick(&engine->Timer);
    R_CameraUpdate(&engine->Camera, ticks_to_seconds_FLOAT(engine->Timer.elapsed_ticks));
    GuiUpdate(engine);
}

static void EngineDraw(Sendai *engine) {
    R_Update(&engine->WorldRenderer, &engine->Camera, &engine->Scene);
    R_Draw(&engine->WorldRenderer, &engine->Scene);
}

void HandleWindowsMessage(Sendai *engine, MSG *msg)
{
	while (PeekMessage(msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg->message == WM_QUIT) {
			engine->bRunning = FALSE;
		}

		TranslateMessage(msg);
		DispatchMessage(msg);
	}
}

void GuiUpdate(Sendai *engine)
{
	UI_InputBegin(&engine->UI);
	UI_LogWindow(&engine->UI);
	UI_InputEnd(&engine->UI);
}