#pragma once

typedef struct S_Log {
	char Buffer[10000];
	int Len;
	int Max;
} S_Log;

extern S_Log SENDAI_LOG;

void S_LogAppend(const char *Text);
void S_LogAppendf(const char *Format, ...);