#pragma once

#define KILOBYTES(val) ((val) * 1024ULL)
#define MEGABYTES(val) (KILOBYTES(val) * 1024ULL)
#define GIGABYTES(val) (MEGABYTES(val) * 1024ULL)

// round val to the nearest multiple of a power-of-two boundary
#define ROUND_UP_POWER_OF_2(val, power_of_two_boundary) ((val + (power_of_two_boundary - 1)) & (~(power_of_two_boundary - 1)))

#define CB_ALIGN(type) ROUND_UP_POWER_OF_2(sizeof(type), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
#define ALIGN_4_BYTES(Val) ROUND_UP_POWER_OF_2(Val, 4)

typedef struct ID3D12Resource ID3D12Resource;

typedef struct M_Arena {
	UINT8 *Base;
	size_t SizeReserved;
	size_t SizeCommitted;
	size_t Offset;
} M_Arena;

M_Arena M_ArenaInit(size_t ReserveSize);
void *M_ArenaAlloc(M_Arena *Arena, size_t Size);
void M_ArenaReset(M_Arena *Arena);
void M_ArenaRelease(M_Arena *Arena);


D3D12_GPU_VIRTUAL_ADDRESS M_GpuAddress(ID3D12Resource *Resource, UINT64 Offset);