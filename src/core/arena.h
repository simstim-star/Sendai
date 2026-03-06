#pragma once

#include <windows.h>

#define KILOBYTES(val) ((val) * 1024ULL)
#define MEGABYTES(val) (KILOBYTES(val) * 1024ULL)
#define GIGABYTES(val) (MEGABYTES(val) * 1024ULL)

typedef struct SendaiArena{
	UINT8 *base;		   // Start of reserved address space
	size_t size_reserved;  // Total size of address space
	size_t size_committed; // Currently committed memory
	size_t offset;		   // Current allocation position
} SendaiArena;

SendaiArena SendaiArena_init(size_t reserve_size);
void *SendaiArena_alloc(SendaiArena *arena, size_t size);
void SendaiArena_reset(SendaiArena *arena);
void SendaiArena_release(SendaiArena *arena);
