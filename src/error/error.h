#pragma once

#include <windows.h>

void ExitIfFailed(const HRESULT hr);
void exit_with_msg(const char *msg);