#pragma once

#include "renderer/renderer.h"
#include "ui/ui.h"
#include "camera.h"
#include "scene.h"
#include "timer.h"

typedef struct Sendai {
	PWSTR Title;
	PWSTR Window;
	HINSTANCE hInstance;
	HWND hWnd;
	BOOL bRunning;

	R_Core RendererCore;
	
	S_UI UI;
	R_Camera Camera;
	S_StepTimer Timer;
	S_Scene Scene;
	UINT FrameCounter;
} Sendai;

INT S_Run(void);

void S_DoNothing(Sendai *const Engine);
void S_FileOpen(Sendai *const Engine);
void S_WireframeMode(Sendai *const Engine);
void S_GridMode(Sendai *const Engine);