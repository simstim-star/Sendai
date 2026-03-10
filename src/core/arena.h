#pragma once

#include "pch.h"

#define KILOBYTES(val) ((val) * 1024ULL)
#define MEGABYTES(val) (KILOBYTES(val) * 1024ULL)
#define GIGABYTES(val) (MEGABYTES(val) * 1024ULL)

typedef struct SendaiArena{
	UINT8 *Base;		   // Start of reserved address space
	size_t SizeReserved;  // Total size of address space
	size_t SizeCommitted; // Currently committed memory
	size_t Offset;		   // Current allocation position
} S_Arena;

S_Arena S_ArenaInit(size_t ReserveSize);
void *S_ArenaAlloc(S_Arena *Arena, size_t Size);
void S_ArenaReset(S_Arena *Arena);
void S_ArenaRelease(S_Arena *Arena);
