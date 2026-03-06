#include "arena.h"
#include "../error/error.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

S_Arena S_ArenaInit(size_t ReserveSize)
{
	S_Arena Arena = {0};
	Arena.Base = VirtualAlloc(NULL, ReserveSize, MEM_RESERVE, PAGE_READWRITE);

	if (!Arena.Base) {
		ExitWithMessage("[ARENA] Failed to reserve virtual memory");
	}

	Arena.SizeReserved = ReserveSize;
	Arena.SizeCommitted = 0;
	Arena.Offset = 0;

	return Arena;
}

void *S_ArenaAlloc(S_Arena *Arena, size_t Size)
{
	size_t AlignMask = 7;
	size_t Position = (Arena->Offset + AlignMask) & ~AlignMask;
	size_t End = Position + Size;

	if (End > Arena->SizeReserved) {
		ExitWithMessage("[ARENA] Arena overflow: Out of reserved virtual address space");
		return NULL;
	}

	if (End > Arena->SizeCommitted) {
		size_t ToCommit = End - Arena->SizeCommitted;
		if (!VirtualAlloc(Arena->Base + Arena->SizeCommitted, ToCommit, MEM_COMMIT, PAGE_READWRITE)) {
			ExitWithMessage("[ARENA] Failed to commit physical memory");
			return NULL;
		}
		Arena->SizeCommitted = End;
	}
	Arena->Offset = End;
	return Arena->Base + Position;
}

void S_ArenaReset(S_Arena *Arena)
{
	Arena->Offset = 0;
	if (Arena->SizeCommitted > 0) {
		VirtualAlloc(Arena->Base, Arena->SizeCommitted, MEM_RESET, PAGE_READWRITE);
	}
}

void S_ArenaRelease(S_Arena *Arena)
{
	if (Arena->Base) {
		VirtualFree(Arena->Base, 0, MEM_RELEASE);
	}
	Arena->Base = NULL;
	Arena->Offset = 0;
	Arena->SizeCommitted = 0;
	Arena->SizeReserved = 0;
}