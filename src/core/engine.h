#pragma once

#include "../renderer/renderer.h"
#include "../gui/gui.h"
#include "camera.h"
#include "timer.h"

typedef struct Sendai {
	CHAR		 *title;
	CHAR		 *curr_window;
	HINSTANCE    hinstance;
	HWND		 hwnd;
	BOOL		 is_running;

	SendaiGui_Context gui;
	Sendai_Camera	  camera;
	Sendai_Renderer   renderer;
	Sendai_Step_Timer timer;
} Sendai;

int Sendai_run();
void Sendai_handle_input(Sendai *engine, MSG *msg);
void SendaiGui_update(Sendai *engine);
void Sendai_destroy(Sendai *engine);
