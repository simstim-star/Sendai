#include "core/pch.h"

#include "error.h"

VOID
ExitIfFailed(const HRESULT hr)
{
	if (!FAILED(hr)) {
		return;
	}

	WCHAR Message[64] = L"";
	if (swprintf(Message, 64, L"ERROR: HRESULT 0x%08X\n", (UINT)hr) > 0) {
		OutputDebugString(Message);
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