#pragma once

#define LOG_CAPACITY 100000

typedef struct S_Log {
	WCHAR Buffer[LOG_CAPACITY];
	char UTF8Buffer[LOG_CAPACITY * 3];
	int Len;
} S_Log;

extern S_Log SENDAI_LOG;

void S_LogAppend(PCWSTR Text);
void S_LogAppendf(PCWSTR Format, ...);