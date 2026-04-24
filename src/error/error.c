#include "core/pch.h"

#include "error.h"

VOID
ExitIfFailed(const HRESULT hr)
{
	if (!FAILED(hr)) {
		return;
	}

	CHAR s_str[64] = "";
	if (snprintf(s_str, 64, "ERROR: HRESULT 0x%08X\n", (UINT)hr) > 0) {
		OutputDebugString(s_str);
	}

	if (IsDebuggerPresent()) {
		__debugbreak();
	}

	exit(EXIT_FAILURE);
}

VOID
ExitWithMessage(PCSTR Message)
{
	fprintf(stderr, "FATAL ERROR: %s\n", Message);

	if (IsDebuggerPresent()) {
		__debugbreak();
	}

	exit(EXIT_FAILURE);
}