#include "win_path.h"

void
Win32CurrPath(_Out_writes_(PathSize) PWSTR const Path, UINT PathSize)
{
	if (Path == NULL) {
		OutputDebugString("Assets path is NULL \n");
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