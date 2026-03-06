#pragma once

#include <windows.h>

void ExitIfFailed(const HRESULT hr);
void ExitWithMessage(const char *Message);