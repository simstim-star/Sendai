#include <stdio.h>

#include "error.h"

void ExitIfFailed(const HRESULT hr)
{
	if (!FAILED(hr)) {
		return;
	}

	char s_str[64] = "";
	if (snprintf(s_str, 64, "ERROR: HRESULT 0x%08X\n", (UINT)hr) > 0) {
		OutputDebugString(s_str);
	}
	exit(EXIT_FAILURE);
}

void ExitWithMessage(const char *Message)
{
	fprintf(stderr, "FATAL ERROR: %s\n", Message);
	exit(EXIT_FAILURE);
}