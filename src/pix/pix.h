#pragma once

typedef struct ID3D12GraphicsCommandList ID3D12GraphicsCommandList;

VOID S_GpuBeginEvent(ID3D12GraphicsCommandList *CommandList, const char *Name);

VOID S_GpuEndEvent(ID3D12GraphicsCommandList *CommandList);