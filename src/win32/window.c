#include "window.h"
#include "../renderer/renderer.h"
#include "../gui/gui.h"
#include "../core/engine.h"

HWND G_HWND = NULL;

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

int win32_run(SC_Engine *const engine, const HINSTANCE hInstance, const int nCmdShow) {
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = window_proc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = "SendaiClass";
	RegisterClassEx(&wc);

	RECT rect = {0, 0, (LONG)(engine->renderer.width), (LONG)(engine->renderer.height)};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	G_HWND = CreateWindow(wc.lpszClassName, engine->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
						  rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, &engine->renderer);

	SC_init(engine);
	ShowWindow(G_HWND, nCmdShow);

	MSG msg;
	engine->is_running = true;
	while (engine->is_running) {
		SC_handle_input(engine, &msg);
		SC_update(engine);
		SR_draw(&engine->renderer);
	}

	SC_destroy(engine);
	return (int)(msg.wParam);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	SR_Renderer *renderer = (SR_Renderer *)(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message) {
	case WM_CREATE: {
		LPCREATESTRUCT pCreateStruct = (LPCREATESTRUCT)(lparam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(pCreateStruct->lpCreateParams));
		return 0;
	}

	case WM_SIZE:
		if (renderer && renderer->swap_chain) {
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			SR_swapchain_resize(renderer, width, height);
			SGUI_resize(width, height);
		}
		return 0;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	if (SGUI_handle_event(hwnd, message, wparam, lparam)) return 0;

	return DefWindowProc(hwnd, message, wparam, lparam);
}