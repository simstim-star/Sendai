#pragma once

#define UTF8_SIZE(StrW) WideCharToMultiByte(CP_UTF8, 0, StrW, -1, NULL, 0, NULL, NULL)
#define W_TO_UTF8(StrW, StrUTF8, UTF8Size) WideCharToMultiByte(CP_UTF8, 0, StrW, -1, StrUTF8, UTF8Size, NULL, NULL)
#define UTF8_TO_W(Str, StrW, WSize) MultiByteToWideChar(CP_UTF8, 0, Str, -1, StrW, WSize)