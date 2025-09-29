#pragma once

#include "../renderer/renderer.h"
#include "../gui/gui.h"
#include "camera.h"

typedef struct snd_engine_t {
	CHAR *title;
	snr_renderer_t renderer;
	sng_gui_t gui;
	snc_camera_t camera;
} snd_engine_t;