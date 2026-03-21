#include "../core/pch.h"
#include <strsafe.h>
#include "win_path.h"

void
Win32CurrPath(_Out_writes_(PathSize) PWSTR const Path, UINT PathSize)
{
	if (Path == NULL) {
		OutputDebugString(L"Assets path is NULL \n");
		exit(EXIT_FAILURE);
	}

	DWORD Size = GetModuleFileNameW(NULL, Path, PathSize);
	if (Size == 0 || Size == PathSize) {
		exit(EXIT_FAILURE);
	}

	PWSTR LastSlash = wcsrchr(Path, L'\\');
	if (LastSlash) {
		*(LastSlash + 1) = L'\0';
	}
}

void
Win32FullPath(PCWSTR SubPath, _Out_writes_(PathSize) PWSTR const Path, UINT PathSize)
{
	Win32CurrPath(Path, PathSize);
	HRESULT hr = StringCchCatW(Path, PathSize, SubPath);
	if (FAILED(hr)) {
		exit(EXIT_FAILURE);
	}
}

void
Win32GetFileNameOnly(PCWSTR FullPath, _Out_writes_(BufferSize) PWSTR const OutBuffer, UINT BufferSize)
{
	if (!FullPath || !OutBuffer || BufferSize == 0) {
		return;
	}

	PCWSTR LastSlash = wcsrchr(FullPath, L'\\');
	PCWSTR FileNameStart = (LastSlash) ? (LastSlash + 1) : FullPath;
	StringCchCopyW(OutBuffer, BufferSize, FileNameStart);
	PWSTR LastDot = wcsrchr(OutBuffer, L'.');
	if (LastDot) {
		*LastDot = L'\0';
	}
}