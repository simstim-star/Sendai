#include "engine.h"
#include "../assets/gltf.h"
#include "../gui/gui.h"
#include "../renderer/renderer.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void init_window(Sendai *engine);
LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static void handle_windows_msg(Sendai *engine, MSG *msg);
static void gui_update(Sendai *engine);

/****************************************************
	Public functions
*****************************************************/

int Sendai_run()
{
	Sendai engine;

	engine.title = "Sendai";
	engine.renderer = (Sendai_Renderer){.width = 1280, .height = 720};
	engine.camera = Sendai_camera_spawn((XMFLOAT3){0, 0, -10});
	engine.camera.yaw = 2 * XM_PI; // just to make it start looking at the box. will remove later

	init_window(&engine);
	SendaiRenderer_init(&engine.renderer, engine.hwnd);
	SendaiGui_init(&engine.gui, engine.renderer.width, engine.renderer.height, engine.renderer.device, engine.renderer.command_list);
	SendaiTimer_init(&engine.timer);

	// SendaiGLTF_load("BoxVertexColors.gltf", &engine.renderer.model);
	SendaiGLTF_load("BoxTextured.gltf", &engine.renderer.model);

	SendaiRenderer_indices(&engine.renderer);
	SendaiRenderer_upload_texture(&engine.renderer, &engine.renderer.model.textures[0], &engine.renderer.model_gpu_texture, &engine.renderer.model_gpu_srv, 0);

	ShowWindow(engine.renderer.hwnd, SW_MAXIMIZE);
	SetForegroundWindow(engine.renderer.hwnd);
	SetFocus(engine.renderer.hwnd);

	MSG msg;
	engine.is_running = true;
	while (engine.is_running) {
		Sendai_tick(&engine.timer);
		float delta_time = ticks_to_seconds_FLOAT(engine.timer.elapsed_ticks);
		Sendai_camera_update(&engine.camera, delta_time);
		handle_windows_msg(&engine, &msg);
		gui_update(&engine);
		SendaiRenderer_update(&engine.renderer, &engine.camera);
		SendaiRenderer_draw(&engine.renderer);
	}

	SendaiRenderer_destroy(&engine.renderer);
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
	wc.lpszClassName = "SendaiClass";
	RegisterClassEx(&wc);

	RECT rect = {0, 0, (LONG)(engine->renderer.width), (LONG)(engine->renderer.height)};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	engine->hwnd = CreateWindow(
		wc.lpszClassName, engine->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL,
		engine->hinstance, NULL);
	SetWindowLongPtr(engine->hwnd, GWLP_USERDATA, (LONG_PTR)engine);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	Sendai *engine = (Sendai *)(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message) {
	case WM_SIZE:
		if (engine && engine->renderer.swap_chain) {
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			SendaiRenderer_swapchain_resize(&engine->renderer, width, height);
			SendaiGui_resize(width, height);
		}
		return 0;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
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

void handle_windows_msg(Sendai *engine, MSG *msg)
{
	SendaiGui_input_begin(&engine->gui);
	while (PeekMessage(msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg->message == WM_QUIT) {
			engine->is_running = FALSE;
		}

		TranslateMessage(msg);
		DispatchMessage(msg);
	}
	SendaiGui_input_end(&engine->gui);
}

void gui_update(Sendai *engine)
{
	SendaiGui_log_window(&engine->gui);
}