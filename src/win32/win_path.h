#pragma once

/******************************************************************************************************************
	Retrieves the path of the executable file of the current process with a last slash ('\\') appended in the end.
	Example: C:\\path\\to\\my\\executable.exe\\
*******************************************************************************************************************/
void Win32CurrPath(_Out_writes_(PathSize) WCHAR *const Path, UINT PathSize);

void Win32FullPath(PCWSTR SubPath, _Out_writes_(PathSize) PWSTR const Path, UINT PathSize);