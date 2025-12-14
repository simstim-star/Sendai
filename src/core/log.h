#pragma once

typedef struct Sendai_Log {
	char buffer[10000];
	int len;
	int max;
} Sendai_Log;

extern Sendai_Log SENDAI_LOG;

void Sendai_Log_append(const char *text);
void Sendai_Log_appendf(const char *format, ...);