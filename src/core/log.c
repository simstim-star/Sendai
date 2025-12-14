#include "log.h"

#include <string.h>
#include <stdio.h> 
#include <stdarg.h> 

Sendai_Log SENDAI_LOG = {
	.buffer = {0},
	.len = 0,
	.max = sizeof(SENDAI_LOG.buffer) 
};

void Sendai_Log_append(const char *text) {
	if (text == NULL) {
		return;
	}

	size_t text_len = strlen(text);
	size_t remainin_space = SENDAI_LOG.max - SENDAI_LOG.len;

	if (text_len + 1 > remainin_space) {
		text_len = remainin_space - 1;
		if (text_len <= 0) {
			return;
		}
	}

	strncat(SENDAI_LOG.buffer, text, text_len);
	SENDAI_LOG.len += text_len;
	SENDAI_LOG.buffer[SENDAI_LOG.len] = '\0';
}

void Sendai_Log_appendf(const char *format, ...) {
	if (format == NULL) {
		return;
	}

	int remaining_space = SENDAI_LOG.max - SENDAI_LOG.len;

	if (remaining_space <= 1) { 
		return;
	}

	va_list args;
	va_start(args, format);
	int chars_would_write = vsnprintf(SENDAI_LOG.buffer + SENDAI_LOG.len, 
									  remaining_space,				  
									  format,							  
									  args								  
	);
	va_end(args);

	if (chars_would_write < 0) return;

	if (chars_would_write < remaining_space) {
		SENDAI_LOG.len += chars_would_write;
	} else {
		SENDAI_LOG.len += remaining_space - 1;
	}

	SENDAI_LOG.buffer[SENDAI_LOG.len] = '\0';
}