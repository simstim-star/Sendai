#include <stdio.h>

#include "error.h"

void exit_if_failed(const HRESULT hr)
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

void exit_with_msg(const char *msg)
{
	fprintf(stderr, "FATAL ERROR: %s\n", msg);
	exit(EXIT_FAILURE);
}