#pragma once

#define LOG_ENTRY_MAX_CHARACTERS 3000

#define LOG_WARNING_RGB 255, 255, 0
#define LOG_ERROR_RGB 255, 0, 0
#define LOG_INFO_RGB 255, 255, 255

typedef enum ELogCategory
{
	ELC_Info,
	ELC_Warning,
	ELC_Error,
} ELogCategory;

typedef struct S_LogEntry
{
	WCHAR Buffer[LOG_ENTRY_MAX_CHARACTERS];
	CHAR UTF8Buffer[LOG_ENTRY_MAX_CHARACTERS * 3];
	INT Len;
	ELogCategory Category;
} S_LogEntry;

VOID S_Log(ELogCategory Category, PCWSTR Text);
VOID S_Logf(ELogCategory Category, PCWSTR Format, ...);
S_LogEntry S_GetLogEntry(INT Index);
INT S_NumLogEntries();