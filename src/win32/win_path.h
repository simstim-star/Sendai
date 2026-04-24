#pragma once

/******************************************************************************************************************
	Retrieves the path of the executable file of the current process with a last slash ('\\') appended in the end.
	Example: C:\\path\\to\\my\\executable.exe\\
*******************************************************************************************************************/
VOID Win32CurrPath(_Out_writes_(PathSize) WCHAR *const Path, UINT PathSize);

VOID Win32FullPath(PCWSTR SubPath, _Out_writes_(PathSize) PWSTR const Path, UINT PathSize);

VOID Win32GetFileNameOnly(PCWSTR FullPath, _Out_writes_(BufferSize) PWSTR const OutBuffer, UINT BufferSize);

VOID Win32AppendFileNameToPath(_In_z_ PWSTR BasePathW,
							   _In_z_ char *FileName,
							   _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH]);

VOID Win32RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH]);