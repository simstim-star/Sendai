#pragma once

#include "../gui/gui.h"
#include "../renderer/renderer.h"
#include "camera.h"
#include "timer.h"
#include "scene.h"

typedef struct Sendai {
	WCHAR *title;
	WCHAR *curr_window;
	HINSTANCE hinstance;
	HWND hwnd;
	BOOL is_running;

	SendaiGui_Renderer gui_renderer;
	Sendai_Camera camera;
	Sendai_WorldRenderer world_renderer;
	Sendai_Step_Timer timer;
	Sendai_Scene scene;
} Sendai;

int Sendai_run();