#include "pch.h"

#include "memory.h"
#include "../error/error.h"

void
M_UpdateResourceData(ID3D12Resource *Resource, const void *Data, size_t DataSize, UINT64 Offset)
{
	UINT8 *Begin = NULL;
	const D3D12_RANGE ReadRange = {0, 0};
	HRESULT hr = ID3D12Resource_Map(Resource, 0, &ReadRange, &Begin);
	ExitIfFailed(hr);
	memcpy(Begin + Offset, Data, DataSize);
	ID3D12Resource_Unmap(Resource, 0, NULL);
}

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

void *
M_ArenaAlloc(M_Arena *Arena, size_t Size)
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

void
M_ArenaReset(M_Arena *Arena)
{
	Arena->Offset = 0;
	if (Arena->SizeCommitted > 0) {
		VirtualAlloc(Arena->Base, Arena->SizeCommitted, MEM_RESET, PAGE_READWRITE);
	}
}

void
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