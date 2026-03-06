#include "arena.h"
#include "../error/error.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

SendaiArena SendaiArena_init(size_t reserve_size)
{
	SendaiArena arena = {0};
	arena.base = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);

	if (!arena.base) {
		exit_with_msg("[ARENA] Failed to reserve virtual memory");
	}

	arena.size_reserved = reserve_size;
	arena.size_committed = 0;
	arena.offset = 0;

	return arena;
}

void *SendaiArena_alloc(SendaiArena *arena, size_t size)
{
	size_t align_mask = 7;
	size_t pos = (arena->offset + align_mask) & ~align_mask;
	size_t end = pos + size;

	if (end > arena->size_reserved) {
		exit_with_msg("[ARENA] Arena overflow: Out of reserved virtual address space");
		return NULL;
	}

	if (end > arena->size_committed) {
		size_t to_commit = end - arena->size_committed;
		if (!VirtualAlloc(arena->base + arena->size_committed, to_commit, MEM_COMMIT, PAGE_READWRITE)) {
			exit_with_msg("[ARENA] Failed to commit physical memory");
			return NULL;
		}
		arena->size_committed = end;
	}
	arena->offset = end;
	return arena->base + pos;
}

void SendaiArena_reset(SendaiArena *arena)
{
	arena->offset = 0;
	if (arena->size_committed > 0) {
		VirtualAlloc(arena->base, arena->size_committed, MEM_RESET, PAGE_READWRITE);
	}
}

void SendaiArena_release(SendaiArena *arena)
{
	if (arena->base) {
		VirtualFree(arena->base, 0, MEM_RELEASE);
	}
	arena->base = NULL;
	arena->offset = 0;
	arena->size_committed = 0;
	arena->size_reserved = 0;
}