#pragma once

#include "../ui/ui.h"
#include "../renderer/renderer.h"
#include "camera.h"
#include "timer.h"
#include "scene.h"

typedef struct Sendai {
	WCHAR *Title;
	WCHAR *Window;
	HINSTANCE hInstance;
	HWND hWnd;
	BOOL bRunning;

	UI_Renderer UI;
	R_Camera Camera;
	R_World WorldRenderer;
	Sendai_Step_Timer Timer;
	SendaiScene Scene;
} Sendai;

int Sendai_run();