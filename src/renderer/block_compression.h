#pragma once

typedef struct R_Texture R_Texture;

BOOL R_TryBlockCompressRGBA8(const R_Texture *const Source, R_Texture *Out, UINT8 **OutBlob);