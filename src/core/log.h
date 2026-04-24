#pragma once

#define LOG_CAPACITY 100000

typedef struct S_Log {
	WCHAR Buffer[LOG_CAPACITY];
	CHAR UTF8Buffer[LOG_CAPACITY * 3];
	INT Len;
} S_Log;

extern S_Log SENDAI_LOG;

VOID S_LogAppend(PCWSTR Text);
VOID S_LogAppendf(PCWSTR Format, ...);