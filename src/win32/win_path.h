#pragma once

#include <sal.h>
#include <wtypes.h>

/******************************************************************************************************************
	Retrieves the path of the executable file of the current process with a last slash ('\\') appended in the end.
	Example: C:\\path\\to\\my\\executable.exe\\
*******************************************************************************************************************/
void win32_curr_path(_Out_writes_(path_size) WCHAR *const path, UINT path_size);