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

void R_CreateCustomTexture(PCWSTR Path, R_Core *Renderer);
void R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex);
GPUTexture R_UploadTexture(R_Core *Renderer, R_Texture *Source);
ID3D12Resource *R_CommandCreateTextureGPU(R_Core *Renderer, R_Texture *Source);
UINT64 R_SuballocateTextureUpload(R_Core *Renderer, UINT64 Size);
UINT32
R_GetTextureIndex(R_Core *Renderer, S_Scene *Scene, int ModelIdx, R_Texture *Texture);
void R_LoadPBRTextures(R_Primitive *Primitive, R_Core *Renderer, S_Scene *Scene, int ModelIdx);