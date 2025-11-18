#pragma once

#include <windows.h>

extern HWND G_HWND;

int win32_run(struct SC_Engine* const engine, const HINSTANCE hInstance, const int nCmdShow);