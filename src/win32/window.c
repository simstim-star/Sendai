#include "window.h"
#include "../renderer/renderer.h"
#include "../core/engine.h"

HWND G_HWND = NULL;

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

int win32_run(snd_engine_t *const engine, const HINSTANCE hInstance, const int nCmdShow) {
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

	snr_init(&engine->renderer, &engine->gui);

	ShowWindow(G_HWND, nCmdShow);

	MSG msg;
	BOOL running = TRUE;
	while (running) {
		sng_input_begin(&engine->gui);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) running = FALSE;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		sng_input_end(&engine->gui);
		
		snr_update(&engine->renderer, &engine->gui);
		snr_draw(&engine->renderer);
	}

	snr_destroy(&engine->renderer);
	return (int)(msg.wParam);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	snr_renderer_t *renderer = (snr_renderer_t *)(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
			snr_swapchain_resize(renderer, width, height);
			sng_resize(width, height);
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

	if (sng_handle_event(hwnd, message, wparam, lparam)) return 0;

	return DefWindowProc(hwnd, message, wparam, lparam);
}