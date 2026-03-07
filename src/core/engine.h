#pragma once

#include "../ui/ui.h"
#include "../renderer/renderer.h"
#include "camera.h"
#include "timer.h"
#include "scene.h"

typedef struct Sendai {
	PWSTR Title;
	PWSTR Window;
	HINSTANCE hInstance;
	HWND hWnd;
	BOOL bRunning;

	UI_Renderer UI;
	R_Camera Camera;
	R_World WorldRenderer;
	S_StepTimer Timer;
	SendaiScene Scene;
} Sendai;

int Sendai_run();