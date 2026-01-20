#include "engine.h"
#include "../assets/gltf.h"
#include "../gui/gui.h"
#include "../renderer/renderer.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void init_window(Sendai *engine);
LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static void engine_update(Sendai *engine);
static void engine_draw(Sendai *engine);
static void handle_windows_msg(Sendai *engine, MSG *msg);
static void gui_update(Sendai *engine);

/****************************************************
	Public functions
*****************************************************/

int Sendai_run()
{
	Sendai engine;

	engine.title = L"Sendai";
	engine.world_renderer = (Sendai_WorldRenderer){.width = 1280, .height = 720};
	engine.camera = Sendai_camera_spawn((XMFLOAT3){0, 0, -10});
	engine.camera.yaw = 2 * XM_PI; // just to make it start looking at the box. will remove later

	init_window(&engine);
	SendaiRenderer_init(&engine.world_renderer, engine.hwnd);
	SendaiGui_init(&engine.gui_renderer, engine.world_renderer.width, engine.world_renderer.height, engine.world_renderer.device, engine.world_renderer.command_list);
	SendaiTimer_init(&engine.timer);

	SendaiGLTF_load("BoxTextured.gltf", &engine.scene);

	for (int i = 0; i < engine.scene.mesh_count; ++i) {
		SendaiRenderer_vertices(engine.world_renderer.device, &engine.scene.meshes[i]);
		SendaiRenderer_indices(engine.world_renderer.device, &engine.scene.meshes[i]);
		SendaiRenderer_upload_texture(
			&engine.world_renderer, &engine.scene.meshes[0].textures[0], &engine.world_renderer.model_gpu_texture, &engine.world_renderer.model_gpu_srv, 0);
	}
	ShowWindow(engine.world_renderer.hwnd, SW_MAXIMIZE);
	SetForegroundWindow(engine.world_renderer.hwnd);
	SetFocus(engine.world_renderer.hwnd);

	MSG msg;
	engine.is_running = true;
	while (engine.is_running) {
		handle_windows_msg(&engine, &msg);
	}

	SendaiRenderer_destroy(&engine.world_renderer);
	return (int)(msg.wParam);
}

/****************************************************
	Implementation of private functions
*****************************************************/

void init_window(Sendai *engine)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_proc;
	wc.hInstance = engine->hinstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
	wc.lpszClassName = L"SendaiClass";
	RegisterClassEx(&wc);

	RECT rect = {0, 0, (LONG)(engine->world_renderer.width), (LONG)(engine->world_renderer.height)};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	engine->hwnd = CreateWindow(
		wc.lpszClassName, engine->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,
		engine->hinstance, engine);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	Sendai *engine = (Sendai *)(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message) {
	case WM_CREATE: 
		{
			LPCREATESTRUCT p_create_struct = (LPCREATESTRUCT)(lparam);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(p_create_struct->lpCreateParams));
		}
		return 0;
	case WM_SIZE:
		if (engine && engine->world_renderer.swap_chain) {
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			SendaiRenderer_swapchain_resize(&engine->world_renderer, width, height);
			SendaiGui_resize(width, height);
		}
		return 0;

	case WM_PAINT: {
		if (engine) {
			engine_draw(engine);
			engine_update(engine);
		}
		// Do I need this? MSFT DX12 examples don't do it
		//PAINTSTRUCT ps;
		//BeginPaint(hwnd, &ps);
		//EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_KEYDOWN:
		if (engine) {
			Sendai_camera_on_key_down(&engine->camera, wparam);
		}
		return 0;

	case WM_KEYUP:
		if (engine) {
			Sendai_camera_on_key_up(&engine->camera, wparam);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	if (SendaiGui_handle_event(hwnd, message, wparam, lparam))
		return 0;

	return DefWindowProc(hwnd, message, wparam, lparam);
}

void engine_update(Sendai *engine)
{
	Sendai_tick(&engine->timer);
	Sendai_camera_update(&engine->camera, ticks_to_seconds_FLOAT(engine->timer.elapsed_ticks));
	gui_update(engine);
}

void engine_draw(Sendai *engine)
{
	SendaiRenderer_update(&engine->world_renderer, &engine->camera, &engine->scene);
	SendaiRenderer_draw(&engine->world_renderer, &engine->scene);
}

void handle_windows_msg(Sendai *engine, MSG *msg)
{
	while (PeekMessage(msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg->message == WM_QUIT) {
			engine->is_running = FALSE;
		}

		TranslateMessage(msg);
		DispatchMessage(msg);
	}
}

void gui_update(Sendai *engine)
{
	SendaiGui_input_begin(&engine->gui_renderer);
	SendaiGui_log_window(&engine->gui_renderer);
	SendaiGui_input_end(&engine->gui_renderer);
}