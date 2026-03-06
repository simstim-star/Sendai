#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

S_Log SENDAI_LOG = {.Buffer = {0}, .Len = 0, .Max = sizeof(SENDAI_LOG.Buffer)};

void S_LogAppend(const char *Text)
{
	if (Text == NULL) {
		return;
	}

	size_t TextLen = strlen(Text);
	size_t RemainingSpace = SENDAI_LOG.Max - SENDAI_LOG.Len;

	if (TextLen + 1 > RemainingSpace) {
		TextLen = RemainingSpace - 1;
		if (TextLen <= 0) {
			return;
		}
	}

	strncat(SENDAI_LOG.Buffer, Text, TextLen);
	SENDAI_LOG.Len += TextLen;
	SENDAI_LOG.Buffer[SENDAI_LOG.Len] = '\0';
}

void S_LogAppendf(const char *Format, ...)
{
	if (Format == NULL) {
		return;
	}

	int RemainingSpace = SENDAI_LOG.Max - SENDAI_LOG.Len;

	if (RemainingSpace <= 1) {
		return;
	}

	va_list Args;
	va_start(Args, Format);
	int CharsWouldWrite = vsnprintf(SENDAI_LOG.Buffer + SENDAI_LOG.Len, RemainingSpace, Format, Args);
	va_end(Args);

	if (CharsWouldWrite < 0)
		return;

	if (CharsWouldWrite < RemainingSpace) {
		SENDAI_LOG.Len += CharsWouldWrite;
	} else {
		SENDAI_LOG.Len += RemainingSpace - 1;
	}

	SENDAI_LOG.Buffer[SENDAI_LOG.Len] = '\0';
}