#pragma once

#include "../renderer/renderer.h"
#include "../gui/gui.h"
#include "camera.h"
#include <stdbool.h>

typedef struct SC_engine_t {
	CHAR *title;
	SR_renderer_t renderer;
	SGUI_context_t gui;
	snc_camera_t camera;
	bool is_running;
	CHAR *curr_window;
} SC_engine_t;

void SC_init(SC_engine_t *engine);
void SC_handle_input(SC_engine_t *engine, MSG *msg);
void SC_update(SC_engine_t *engine);
void SC_destroy(SC_engine_t *engine);
