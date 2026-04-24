#include "pch.h"

#include "log.h"

S_Log SENDAI_LOG = {.Buffer = {0}, .Len = 0};

VOID
S_LogAppend(PCWSTR Text)
{
	if (Text == NULL) {
		return;
	}

	HRESULT hr = StringCchCatW(SENDAI_LOG.Buffer, LOG_CAPACITY, Text);

	if (SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
		size_t NewLen = 0;
		if (SUCCEEDED(StringCchLengthW(SENDAI_LOG.Buffer, LOG_CAPACITY, &NewLen))) {
			SENDAI_LOG.Len = (INT)NewLen;
		}
	}
}

VOID
S_LogAppendf(PCWSTR Format, ...)
{
	if (Format == NULL) {
		return;
	}

	va_list Args;
	va_start(Args, Format);
	HRESULT hr = StringCchVPrintfW(SENDAI_LOG.Buffer + SENDAI_LOG.Len, LOG_CAPACITY - SENDAI_LOG.Len, Format, Args);
	va_end(Args);

	if (SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
		size_t NewLen = 0;
		if (SUCCEEDED(StringCchLengthW(SENDAI_LOG.Buffer, LOG_CAPACITY, &NewLen))) {
			SENDAI_LOG.Len = (INT)NewLen;
		}
	}
}