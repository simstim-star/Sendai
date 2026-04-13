#include "../core/pch.h"

#include "../error/error.h"
#include "../ui/ui.h"
#include "../win32/str_helper.h"
#include "renderer.h"
#include "texture.h"

#define STB_DS_IMPLEMENTATION
#include "../../deps/stb_ds.h"
#include "../../deps/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../deps/stb_image_resize2.h"

#include "../assets/dds_loader.h"

static const UINT8 WHITE_PIXEL[] = {255, 255, 255, 255};

static const R_Texture WhiteTexture = {.Name = "fallback_white", .Width = 1, .Height = 1, .MipLevels = 1, .MipPixels[0] = WHITE_PIXEL};

void
R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex)
{
	char PathUTF8[MAX_PATH * 4];
	W_TO_UTF8(Path, PathUTF8, UTF8_SIZE(Path));

	if (shgeti(Renderer->Textures, PathUTF8) != -1) {
		return;
	}

	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){
	  .Height = H,
	  .Width = W,
	  .Name = PathUTF8,
	  .MipLevels = 1,
	  .MipPixels[0] = Pixels,
	};

	GPUTexture NewTex = {0};
	NewTex.GpuTexture = R_CommandCreateTextureGPU(Renderer, &Source);
	UI_SetTextureInNkHeap(nkSlotIndex, NewTex.GpuTexture);

	TextureLookup Lookup = {.key = PathUTF8, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	stbi_image_free(Pixels);
	R_ExecuteCommands(Renderer, Renderer->UploadCommandList, Renderer->UploadCommandAllocator);
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

	D3D12_CPU_DESCRIPTOR_HANDLE CpuDescHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &CpuDescHandle);
	CpuDescHandle.ptr += (SIZE_T)SlotIndex * Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = Source->MipLevels,
	};

	ID3D12Device_CreateShaderResourceView(Renderer->Device, NewTex.GpuTexture, &SrvDesc, CpuDescHandle);

	TextureLookup Lookup = {.key = Source->Name, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);
	R_ExecuteCommands(Renderer, Renderer->UploadCommandList, Renderer->UploadCommandAllocator);
	return NewTex;
}

void
R_CreateCustomTexture(PCWSTR Path, R_Core *Renderer)
{
	char PathUTF8[MAX_PATH * 4];
	W_TO_UTF8(Path, PathUTF8, UTF8_SIZE(Path));
	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){.Height = H, .Width = W, .Name = PathUTF8, .MipLevels = 1, .MipPixels[0] = Pixels};
	R_UploadTexture(Renderer, &Source);
	stbi_image_free(Pixels);
	R_ExecuteCommands(Renderer, Renderer->UploadCommandList, Renderer->UploadCommandAllocator);
}

ID3D12Resource *
R_CommandCreateTextureGPU(R_Core *const Renderer, const R_Texture *const SourceTexture)
{
	D3D12_RESOURCE_DESC TexDesc = {
	  .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	  .Width = SourceTexture->Width,
	  .Height = SourceTexture->Height,
	  .DepthOrArraySize = 1,
	  .MipLevels = (UINT16)SourceTexture->MipLevels,
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc = {1, 0},
	  .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
	  .Flags = D3D12_RESOURCE_FLAG_NONE,
	};
	ID3D12Resource *Texture;
	D3D12_HEAP_PROPERTIES HeapDefault = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	HRESULT hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapDefault, D3D12_HEAP_FLAG_NONE, &TexDesc,
													  D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, &Texture);
	ExitIfFailed(hr);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[D3D12_REQ_MIP_LEVELS];
	UINT NumRows[D3D12_REQ_MIP_LEVELS];
	UINT64 RowSizeInBytes[D3D12_REQ_MIP_LEVELS];
	UINT64 TotalUploadSize = 0;

	ID3D12Device_GetCopyableFootprints(Renderer->Device, &TexDesc, 0, SourceTexture->MipLevels, 0, Layouts, NumRows, RowSizeInBytes,
									   &TotalUploadSize);

	UINT64 Offset = R_SuballocateTextureUpload(Renderer, TotalUploadSize);
	UINT8 *pUploadBufferBase = Renderer->TextureUploadBuffer.BaseMappedPtr + Offset;

	for (uint32_t MipLevel = 0; MipLevel < SourceTexture->MipLevels; MipLevel++) {
		UINT8 *pDestination = pUploadBufferBase + Layouts[MipLevel].Offset;
		UINT8 *pSource = (UINT8 *)SourceTexture->MipPixels[MipLevel];
		for (UINT Row = 0; Row < NumRows[MipLevel]; Row++) {
			memcpy(pDestination + (Row * Layouts[MipLevel].Footprint.RowPitch), pSource + (Row * RowSizeInBytes[MipLevel]),
				   RowSizeInBytes[MipLevel]);
		}
		D3D12_TEXTURE_COPY_LOCATION DestinationLocation = {
		  .pResource = Texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = MipLevel};
		D3D12_TEXTURE_COPY_LOCATION SourceLocation = {.pResource = Renderer->TextureUploadBuffer.Buffer,
													  .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
													  .PlacedFootprint = Layouts[MipLevel]};
		SourceLocation.PlacedFootprint.Offset += Offset;
		ID3D12GraphicsCommandList_CopyTextureRegion(Renderer->UploadCommandList, &DestinationLocation, 0, 0, 0, &SourceLocation, NULL);
	}

	D3D12_RESOURCE_BARRIER Barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition = {
										.pResource = Texture,
										.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
										.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
										.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->UploadCommandList, 1, &Barrier);

	return Texture;
}

UINT64
R_SuballocateTextureUpload(R_Core *const Renderer, UINT64 Size)
{
	UINT64 AlignedOffset = ROUND_UP_POWER_OF_2(Renderer->TextureUploadBuffer.CurrentOffset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	
	if (AlignedOffset + Size > Renderer->TextureUploadBuffer.Size) {
		assert(Size <= Renderer->TextureUploadBuffer.Size);
		R_ExecuteCommands(Renderer, Renderer->UploadCommandList, Renderer->UploadCommandAllocator);
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
	if (Texture && Texture->Name) {
		Target = *Texture;
	} else {
		Target = WhiteTexture;
	}

	GPUTexture Tex = R_UploadTexture(Renderer, &Target);
	return Tex.HeapIndex;
}

UINT
R_CalculateMipLevels(INT Width, INT Height)
{
	return 1 + (uint32_t)floorf(log2f((float)max(Width, Height)));
}

void
R_GenerateMips(R_Model *Model, M_Arena *UploadArena)
{
	for (size_t i = 0; i < Model->ImagesCount; ++i) {
		R_Texture *Texture = &Model->Images[i];

		if (Texture->MipPixels[0] == NULL || Texture->MipLevels <= 1) {
			continue;
		}

		INT CurrentWidth = Texture->Width;
		INT CurrentHeight = Texture->Height;

		for (size_t MipLevel = 1; MipLevel < Texture->MipLevels; MipLevel++) {
			INT NextWidth = CurrentWidth > 1 ? CurrentWidth / 2 : 1;
			INT NextHeight = CurrentHeight > 1 ? CurrentHeight / 2 : 1;

			Texture->MipPixels[MipLevel] = M_ArenaAlloc(UploadArena, NextWidth * NextHeight * 4);

			stbir_resize_uint8_linear((unsigned char *)Texture->MipPixels[MipLevel - 1], CurrentWidth, CurrentHeight, 0,
									  (unsigned char *)Texture->MipPixels[MipLevel], NextWidth, NextHeight, 0, STBIR_RGBA);

			CurrentWidth = NextWidth;
			CurrentHeight = NextHeight;
		}
	}
}

void
R_UploadDDSResource(R_Core *const Renderer, ID3D12Resource *Texture, D3D12_SUBRESOURCE_DATA *Subresources, UINT MipLevels)
{
	D3D12_RESOURCE_DESC TexDesc;
	ID3D12Resource_GetDesc(Texture, &TexDesc);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[D3D12_REQ_MIP_LEVELS];
	UINT NumRows[D3D12_REQ_MIP_LEVELS];
	UINT64 RowSizeInBytes[D3D12_REQ_MIP_LEVELS];
	UINT64 TotalUploadSize = 0;

	ID3D12Device_GetCopyableFootprints(Renderer->Device, &TexDesc, 0, MipLevels, 0, Layouts, NumRows, RowSizeInBytes, &TotalUploadSize);

	UINT64 Offset = R_SuballocateTextureUpload(Renderer, TotalUploadSize);
	UINT8 *pUploadBufferBase = Renderer->TextureUploadBuffer.BaseMappedPtr + Offset;

	for (UINT MipLevel = 0; MipLevel < MipLevels; MipLevel++) {
		UINT8 *pDestination = pUploadBufferBase + Layouts[MipLevel].Offset;
		const UINT8 *pSource = (const UINT8 *)Subresources[MipLevel].pData;

		for (UINT Row = 0; Row < NumRows[MipLevel]; Row++) {
			memcpy(pDestination + (Row * Layouts[MipLevel].Footprint.RowPitch), pSource + (Row * Subresources[MipLevel].RowPitch), RowSizeInBytes[MipLevel]);
		}

		D3D12_TEXTURE_COPY_LOCATION DestLoc = {.pResource = Texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = MipLevel};
		D3D12_TEXTURE_COPY_LOCATION SrcLoc = {
		  .pResource = Renderer->TextureUploadBuffer.Buffer, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = Layouts[MipLevel]};
		SrcLoc.PlacedFootprint.Offset += Offset;

		ID3D12GraphicsCommandList_CopyTextureRegion(Renderer->UploadCommandList, &DestLoc, 0, 0, 0, &SrcLoc, NULL);
	}

	D3D12_RESOURCE_BARRIER Barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition = {
										.pResource = Texture,
										.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
										.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
										.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->UploadCommandList, 1, &Barrier);
}

GPUTexture
R_UploadTextureFromDDSFile(R_Core *const Renderer, const PWSTR FileName, const PSTR NameKey)
{
	ptrdiff_t Index = shgeti(Renderer->Textures, NameKey);
	if (Index != -1) {
		return Renderer->Textures[Index].Texture;
	}

	ID3D12Resource *Texture = NULL;
	UINT8 *DdsData = NULL;
	D3D12_SUBRESOURCE_DATA *Subresources = NULL;
	arrsetlen(Subresources, D3D12_REQ_MIP_LEVELS);


	const int POTATO_COMPUTER = 1024;
	HRESULT hr = LoadDDSTextureFromFile(Renderer->Device, FileName, &Texture, &DdsData, Subresources, POTATO_COMPUTER, NULL, NULL);
	ExitIfFailed(hr);

	D3D12_RESOURCE_DESC Desc;
	ID3D12Resource_GetDesc(Texture, &Desc);

	R_UploadDDSResource(Renderer, Texture, Subresources, Desc.MipLevels);

	uint32_t SlotIndex = Renderer->TexturesCount++;
	GPUTexture NewTex = {
	  .GpuTexture = Texture,
	  .HeapIndex = SlotIndex,
	};

	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &CpuHandle);
	CpuHandle.ptr += (SIZE_T)SlotIndex * Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {
	  .Format = Desc.Format,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = Desc.MipLevels,
	};
	ID3D12Device_CreateShaderResourceView(Renderer->Device, Texture, &SrvDesc, CpuHandle);

	TextureLookup Lookup = {.key = NameKey, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	R_ExecuteCommands(Renderer, Renderer->UploadCommandList, Renderer->UploadCommandAllocator);
	arrfree(Subresources);
	free(DdsData);

	return NewTex;
}