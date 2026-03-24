#pragma once

#include "../../deps/nuklear.h"
#include "../shaders/nuklear/nuklear_d3d12.h"

#define MAX_VERTEX_BUFFER (512 * 1024)
#define MAX_INDEX_BUFFER (128 * 1024)

#define COLOR_DISABLED nk_rgba(100, 100, 100, 255)

typedef enum { UI_ACTION_NONE = 0, UI_ACTION_FILE_OPEN, UI_ACTION_WIREFRAME_BUTTON_CLICKED } UI_EAction;
typedef enum { UI_EUT_WIREFRAME, UI_EUT_CAMERA, UI_EUT_NUM_USER_TEXTURES } UI_EUserTextures;