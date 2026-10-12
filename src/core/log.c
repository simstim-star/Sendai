#include "pch.h"

#include "log.h"

#include <assert.h>

#define MAX_NUM_LOGS 2000

typedef struct S_Logs
{
	S_LogEntry Entry[MAX_NUM_LOGS];
	INT LastIndex;
} S_Logs;

S_Logs SENDAI_LOG = {.LastIndex = 0};

VOID
S_Log(ELogCategory Category, PCWSTR Text)
{
	if (Text == NULL || SENDAI_LOG.LastIndex >= MAX_NUM_LOGS)
	{
		return;
	}

	S_LogEntry *Entry = &SENDAI_LOG.Entry[SENDAI_LOG.LastIndex];
	size_t UnusedCharsInBufferRemaining = 0;
	StringCchCopyExW(Entry->Buffer, LOG_ENTRY_MAX_CHARACTERS, Text, NULL, &UnusedCharsInBufferRemaining, 0);
	Entry->Len = (INT)(LOG_ENTRY_MAX_CHARACTERS - UnusedCharsInBufferRemaining);

	INT RequiredBytes =
		WideCharToMultiByte(CP_UTF8, 0, Entry->Buffer, Entry->Len, Entry->UTF8Buffer, sizeof(Entry->UTF8Buffer) - 1, NULL, NULL);
	if (RequiredBytes >= 0)
	{
		Entry->UTF8Buffer[RequiredBytes] = '\0';
	}

	Entry->Category = Category;
}

VOID
S_Logf(ELogCategory Category, PCWSTR Format, ...)
{
	if (Format == NULL || SENDAI_LOG.LastIndex >= MAX_NUM_LOGS)
	{
		return;
	}

	S_LogEntry *Entry = &SENDAI_LOG.Entry[SENDAI_LOG.LastIndex];
	size_t UnusedCharsInBufferRemaining = LOG_ENTRY_MAX_CHARACTERS;

	va_list Args;
	va_start(Args, Format);
	StringCchVPrintfExW(Entry->Buffer, LOG_ENTRY_MAX_CHARACTERS, NULL, &UnusedCharsInBufferRemaining, 0, Format, Args);
	va_end(Args);
	
	Entry->Len = (INT)(LOG_ENTRY_MAX_CHARACTERS - UnusedCharsInBufferRemaining);

	INT RequiredBytes =
		WideCharToMultiByte(CP_UTF8, 0, Entry->Buffer, Entry->Len, Entry->UTF8Buffer, sizeof(Entry->UTF8Buffer) - 1, NULL, NULL);
	if (RequiredBytes >= 0)
	{
		Entry->UTF8Buffer[RequiredBytes] = '\0';
	}

	Entry->Category = Category;
	SENDAI_LOG.LastIndex++;
}

S_LogEntry
S_GetLogEntry(INT Index)
{
	if (Index >= SENDAI_LOG.LastIndex)
	{
		return;
	}

	return SENDAI_LOG.Entry[Index];
}

INT
S_NumLogEntries()
{
	return SENDAI_LOG.LastIndex;
}
