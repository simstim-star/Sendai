#pragma once

#include <windows.h>

extern HWND G_HWND;

int win32_run(struct snd_engine_t* const renderer, const HINSTANCE hInstance, const int nCmdShow);