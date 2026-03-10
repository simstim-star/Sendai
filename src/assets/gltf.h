#pragma once

#include "../core/pch.h"

typedef struct SendaiScene SendaiScene;

BOOL SendaiGLTF_LoadModel(PCWSTR path, SendaiScene *out_scene);