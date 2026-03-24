#include "../core/pch.h"

#include "../error/error.h"
#include "../ui/ui.h"
#include "renderer.h"
#include "texture.h"

#define STB_DS_IMPLEMENTATION
#include "../../deps/stb_ds.h"
#include "../../deps/stb_image.h"

static const UINT8 WHITE_PIXEL[] = {255, 255, 255, 255};

static const R_Texture WhiteTexture = {
  .Name = "fallback_black",
  .Pixels = WHITE_PIXEL,
  .Width = 1,
  .Height = 1,
};

void
R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex)
{
	char PathUTF8[MAX_PATH * 4];
	WideCharToMultiByte(CP_UTF8, 0, Path, -1, PathUTF8, (INT)sizeof(PathUTF8), NULL, NULL);

	if (shgeti(Renderer->Textures, PathUTF8) != -1) {
		return;
	}

	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){
	  .Height = H,
	  .Width = W,
	  .Pixels = Pixels,
	  .Name = PathUTF8,
	};

	GPUTexture NewTex = {0};
	NewTex.GpuTexture = R_CommandCreateTextureGPU(Renderer, &Source);
	UI_SetTextureInNkHeap(nkSlotIndex, NewTex.GpuTexture);

	TextureLookup Lookup = {.key = PathUTF8, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	stbi_image_free(Pixels);
}

GPUTexture
R_UploadTexture(R_Core *const Renderer, const R_Texture *const Source)
{
	ptrdiff_t Index = shgeti(Renderer->Textures, Source->Name);
	if (Index != -1) {
		return Renderer->Textures[Index].Texture;
	}

	uint32_t SlotIndex = Renderer->TexturesCount++;
	GPUTexture NewTex = {
	  .GpuTexture = R_CommandCreateTextureGPU(Renderer, Source),
	  .HeapIndex = SlotIndex,
	};

	UINT IncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE CpuDescHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &CpuDescHandle);
	CpuDescHandle.ptr += (SIZE_T)SlotIndex * IncrementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = 1,
	};
	ID3D12Device_CreateShaderResourceView(Renderer->Device, NewTex.GpuTexture, &SrvDesc, CpuDescHandle);

	TextureLookup Lookup = {.key = Source->Name, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	return NewTex;
}

void
R_CreateCustomTexture(PCWSTR Path, R_Core *Renderer)
{
	char PathUTF8[MAX_PATH * 4];
	WideCharToMultiByte(CP_UTF8, 0, Path, -1, PathUTF8, (INT)sizeof(PathUTF8), NULL, NULL);
	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){.Height = H, .Width = W, .Pixels = Pixels, .Name = PathUTF8};
	R_UploadTexture(Renderer, &Source);
	stbi_image_free(Pixels);
}

ID3D12Resource *
R_CommandCreateTextureGPU(R_Core *const Renderer, const R_Texture *const Source)
{
	ID3D12Resource *Texture = NULL;
	D3D12_RESOURCE_DESC TexDesc = {.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
								   .Width = Source->Width,
								   .Height = Source->Height,
								   .DepthOrArraySize = 1,
								   .MipLevels = 1,
								   .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
								   .SampleDesc = {1, 0},
								   .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
								   .Flags = D3D12_RESOURCE_FLAG_NONE};

	D3D12_HEAP_PROPERTIES HeapDefault = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	HRESULT hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapDefault, D3D12_HEAP_FLAG_NONE, &TexDesc, D3D12_RESOURCE_STATE_COMMON,
													  NULL, &IID_ID3D12Resource, &Texture);
	ExitIfFailed(hr);

	UINT NumRows;
	UINT64 RowSize;
	UINT64 TotalUploadSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;

	ID3D12Device_GetCopyableFootprints(Renderer->Device, &TexDesc, 0, 1, 0, &Footprint, &NumRows, &RowSize, &TotalUploadSize);
	UINT64 Offset = R_SuballocateTextureUpload(Renderer, TotalUploadSize);
	Footprint.Offset += Offset;
	UINT8 *DestPtr = Renderer->TextureUploadBuffer.BaseMappedPtr + Footprint.Offset;
	for (UINT i = 0; i < NumRows; ++i) {
		memcpy(DestPtr + i * Footprint.Footprint.RowPitch, (UINT8 *)Source->Pixels + i * (Source->Width * 4), Source->Width * 4);
	}
	D3D12_TEXTURE_COPY_LOCATION DstLocation = {.pResource = Texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
	D3D12_TEXTURE_COPY_LOCATION SrcLocation = {
	  .pResource = Renderer->TextureUploadBuffer.Buffer, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = Footprint};
	D3D12_RESOURCE_BARRIER ToCopyDest = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
										 .Transition = {
										   .pResource = Texture,
										   .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
										   .StateBefore = D3D12_RESOURCE_STATE_COMMON,
										   .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
										 }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &ToCopyDest);
	ID3D12GraphicsCommandList_CopyTextureRegion(Renderer->CommandList, &DstLocation, 0, 0, 0, &SrcLocation, NULL);
	D3D12_RESOURCE_BARRIER Barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition = {
										.pResource = Texture,
										.Subresource = 0,
										.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
										.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &Barrier);
	return Texture;
}

UINT64
R_SuballocateTextureUpload(R_Core *const Renderer, UINT64 Size)
{
	UINT64 AlignedOffset = ROUND_UP_POWER_OF_2(Renderer->TextureUploadBuffer.CurrentOffset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	if (AlignedOffset + Size > Renderer->TextureUploadBuffer.Size) {
		R_ExecuteCommands(Renderer);
		Renderer->TextureUploadBuffer.CurrentOffset = Size;
		return 0;
	}

	Renderer->TextureUploadBuffer.CurrentOffset = AlignedOffset + Size;
	return AlignedOffset;
}

UINT32
R_GetTextureIndex(R_Core *const Renderer, const R_Texture *const Texture)
{
	R_Texture Target;
	if (Texture) {
		Target = *Texture;
	} else {
		Target = WhiteTexture;
	}

	GPUTexture Tex = R_UploadTexture(Renderer, &Target);
	return Tex.HeapIndex;
}

void
R_LoadPBRTextures(R_Primitive *const Primitive, R_Core *const Renderer, S_Scene *const Scene)
{
	Primitive->cb.AlbedoTextureIndex = R_GetTextureIndex(Renderer, Primitive->Albedo);
	Primitive->cb.NormalTextureIndex = R_GetTextureIndex(Renderer, Primitive->Normal);
	Primitive->cb.MetallicTextureIndex = R_GetTextureIndex(Renderer, Primitive->Metallic);
	Primitive->cb.RoughnessTextureIndex = R_GetTextureIndex(Renderer, Primitive->Roughness);
	Primitive->cb.OcclusionTextureIndex = R_GetTextureIndex(Renderer, Primitive->Occlusion);
	Primitive->cb.EmissiveTextureIndex = R_GetTextureIndex(Renderer, Primitive->Emissive);
	M_ArenaReset(&Scene->UploadArena);
}