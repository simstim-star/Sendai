#include "win_path.h"

void win32_curr_path(_Out_writes_(path_size) WCHAR *const path, UINT path_size) {
	if (path == NULL) {
		OutputDebugString("Assets path is NULL \n");
		exit(EXIT_FAILURE);
	}

	DWORD size = GetModuleFileNameW(NULL, path, path_size);
	if (size == 0 || size == path_size) {
		exit(EXIT_FAILURE);
	}

	WCHAR *last_slash = wcsrchr(path, L'\\');
	if (last_slash) {
		*(last_slash + 1) = L'\0';
	}
}