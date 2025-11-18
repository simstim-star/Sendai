#include "engine.h"
#include "../renderer/renderer.h"
#include "../gui/gui.h"

void SC_init(SC_Engine *engine) {
	SR_init(&engine->renderer);
	SGUI_init(&engine->gui, engine->renderer.width, engine->renderer.height, engine->renderer.device, engine->renderer.command_list);
	SR_execute_commands(&engine->renderer);
}

void SC_handle_input(SC_Engine *engine, MSG *msg) {
	SGUI_input_begin(&engine->gui);
	while (PeekMessage(msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg->message == WM_QUIT) {
			engine->is_running = FALSE;
		}

		TranslateMessage(msg);
		DispatchMessage(msg);
	}
	SGUI_input_end(&engine->gui);
}

void SC_update(SC_Engine *engine) {
	SR_update(&engine->renderer);
	SGUI_draw_top_bar(&engine->gui, &engine->curr_window);
	if (engine->curr_window && strcmp(engine->curr_window, "triangle") == 0) {
		SGUI_update_triangle_menu(&engine->gui, engine->renderer.data);
	}
}

void SC_destroy(SC_Engine *engine) {
	SR_destroy(&engine->renderer);
}
