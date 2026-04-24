#include "pch.h"

#include "error/error.h"
#include "memory.h"

M_Arena
M_ArenaInit(size_t ReserveSize)
{
	M_Arena Arena = {0};
	Arena.Base = VirtualAlloc(NULL, ReserveSize, MEM_RESERVE, PAGE_READWRITE);

	if (!Arena.Base) {
		ExitWithMessage("[ARENA] Failed to reserve virtual memory");
	}

	Arena.SizeReserved = ReserveSize;
	Arena.SizeCommitted = 0;
	Arena.Offset = 0;

	return Arena;
}

VOID *
M_ArenaAlloc(M_Arena *Arena, size_t Size)
{
	if (Size == 0) {
		return NULL;
	}

	size_t Position = ROUND_UP_POWER_OF_2(Arena->Offset, 8);
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

VOID
M_ArenaReset(M_Arena *Arena)
{
	Arena->Offset = 0;
	if (Arena->SizeCommitted > 0) {
		VirtualAlloc(Arena->Base, Arena->SizeCommitted, MEM_RESET, PAGE_READWRITE);
	}
}

VOID
M_ArenaRelease(M_Arena *Arena)
{
	if (Arena->Base) {
		VirtualFree(Arena->Base, 0, MEM_RELEASE);
	}
	Arena->Base = NULL;
	Arena->Offset = 0;
	Arena->SizeCommitted = 0;
	Arena->SizeReserved = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS
M_GpuAddress(ID3D12Resource *Resource, UINT64 Offset) { return ID3D12Resource_GetGPUVirtualAddress(Resource) + Offset; }
