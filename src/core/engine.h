#pragma once

#include "../renderer/renderer.h"
#include "../gui/gui.h"

typedef struct snd_engine_t {
	CHAR *title;
	snr_renderer_t renderer;
	sng_gui_t gui;
} snd_engine_t;