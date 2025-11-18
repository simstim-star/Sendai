#pragma once

#include "../renderer/renderer.h"
#include "../gui/gui.h"
#include "camera.h"
#include <stdbool.h>

typedef struct SC_Engine {
	CHAR *title;
	SR_Renderer renderer;
	SGUI_Context gui;
	SC_Camera camera;
	bool is_running;
	CHAR *curr_window;
} SC_Engine;

void SC_init(SC_Engine *engine);
void SC_handle_input(SC_Engine *engine, MSG *msg);
void SC_update(SC_Engine *engine);
void SC_destroy(SC_Engine *engine);
