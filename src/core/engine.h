#pragma once

#include "../renderer/renderer.h"
#include "../ui/ui.h"
#include "camera.h"
#include "scene.h"
#include "timer.h"

typedef struct Sendai {
	PWSTR Title;
	PWSTR Window;
	HINSTANCE hInstance;
	HWND hWnd;
	BOOL bRunning;

	UI_Renderer UI_Renderer;
	UI_State UI;
	R_Camera Camera;
	R_World WorldRenderer;
	S_StepTimer Timer;
	SendaiScene Scene;
	UINT FrameCounter;
} Sendai;

int Sendai_run();