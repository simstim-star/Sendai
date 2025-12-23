#pragma once

#include "../gui/gui.h"
#include "../renderer/renderer.h"
#include "camera.h"
#include "timer.h"

typedef struct Sendai {
	WCHAR *title;
	WCHAR *curr_window;
	HINSTANCE hinstance;
	HWND hwnd;
	BOOL is_running;

	SendaiGui_Context gui;
	Sendai_Camera camera;
	Sendai_Renderer renderer;
	Sendai_Step_Timer timer;
} Sendai;

int Sendai_run();