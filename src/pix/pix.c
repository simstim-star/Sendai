#include "pix.h"

#define PIX_EVENT_ANSI_VERSION 1

VOID
S_GpuBeginEvent(ID3D12GraphicsCommandList *CommandList, const char *Name)
{
	ID3D12GraphicsCommandList_BeginEvent(CommandList, PIX_EVENT_ANSI_VERSION, Name, (UINT)strlen(Name) + 1);
}

VOID
S_GpuEndEvent(ID3D12GraphicsCommandList *CommandList)
{
	ID3D12GraphicsCommandList_EndEvent(CommandList);
}
