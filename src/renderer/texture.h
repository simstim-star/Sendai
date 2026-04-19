#pragma once

typedef struct R_Core R_Core;
typedef struct R_Texture R_Texture;
typedef struct R_Primitive R_Primitive;
typedef struct S_Scene S_Scene;

typedef struct GPUTexture {
	ID3D12Resource *GpuTexture;
	UINT HeapIndex;
} GPUTexture;

typedef struct TextureLookup {
	char *key;
	GPUTexture Texture;
} TextureLookup;

VOID R_CreateCustomTexture(PCWSTR Path, R_Core *Renderer);
VOID R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex);
GPUTexture R_UploadTexture(R_Core *const Renderer, const R_Texture *const Source);
UINT R_CalculateMipLevels(INT Width, INT Height);
ID3D12Resource *R_CommandCreateTextureGPU(R_Core *const Renderer, const R_Texture *const Source);
UINT64 R_SuballocateTextureUpload(R_Core *Renderer, UINT64 Size);
UINT32
R_GetTextureIndex(R_Core *const Renderer, const R_Texture *const Texture);
VOID R_GenerateMips(R_Model *Model, M_Arena *UploadArena);

VOID R_UploadDDSResource(R_Core *const Renderer, ID3D12Resource *Texture, D3D12_SUBRESOURCE_DATA *Subresources, UINT MipLevels);
GPUTexture R_UploadTextureFromDDSFile(R_Core *const Renderer, const PWSTR FileName, const PSTR NameKey);

GPUTexture R_CreateHDRTexture(PCWSTR Path, R_Core *Renderer);