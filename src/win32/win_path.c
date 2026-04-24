#include "core/pch.h"

#include "win_path.h"
#include <strsafe.h>

VOID
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

VOID
Win32FullPath(PCWSTR SubPath, _Out_writes_(PathSize) PWSTR const Path, UINT PathSize)
{
	Win32CurrPath(Path, PathSize);
	HRESULT hr = StringCchCatW(Path, PathSize, SubPath);
	if (FAILED(hr)) {
		exit(EXIT_FAILURE);
	}
}

VOID
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

VOID
Win32AppendFileNameToPath(_In_z_ PWSTR BasePathW, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH])
{
	char BasePart[MAX_PATH];
	WideCharToMultiByte(CP_UTF8, 0, BasePathW, -1, BasePart, MAX_PATH, NULL, NULL);
	strcpy_s(FullPath, MAX_PATH, BasePart);

	size_t len = strlen(FullPath);
	if (len > 0 && FullPath[len - 1] != '\\' && FullPath[len - 1] != '/') {
		strcat_s(FullPath, MAX_PATH, "\\");
	}

	strcat_s(FullPath, MAX_PATH, FileName);
}

VOID
Win32RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH])
{
	PWSTR LastDoubleSlash = wcsrchr(FullPathBuffer, L'\\');
	PWSTR LastSlash = wcsrchr(FullPathBuffer, L'/');
	PWSTR Separator = (LastDoubleSlash > LastSlash) ? LastDoubleSlash : LastSlash;

	if (Separator) {
		*(Separator + 1) = L'\0'; // Null-terminate after the slash to keep the directory
	} else {
		FullPathBuffer[0] = L'\0'; // No slash found, file is in current working directory
	}
}