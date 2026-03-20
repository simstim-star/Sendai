#include "../core/pch.h"

#include "render_types.h"
#include "renderer.h"
#include "shader.h"

#include "../core/camera.h"
#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../ui/ui.h"
#include "../win32/win_path.h"

#define STB_DS_IMPLEMENTATION
#include "../../deps/stb_ds.h"

#include "../../deps/stb_image.h"

#define MAX_TEXTURES 4096

static const FLOAT CLEAR_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};

typedef enum EReservedSrvIndex {
	ERSI_BILLBOARD_LAMP = 0,
} EReservedSrvIndex;

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers);
static void CreateSceneResources(const R_Core *const Renderer);
static void CreateDepthStencilBuffer(R_Core *const Renderer);
static void CreateShaders(R_Core *const Renderer);
static ID3D12Resource *CommandCreateTextureGPU(R_Core *Renderer, R_Texture *Source);
static UINT64 SuballocateTextureUpload(R_Core *Renderer, UINT64 Size);
static void UpdateResourceData(ID3D12Resource *Resource, const void *Data, size_t DataSize, UINT64 Offset);
static void CreateCustomTexture(PCWSTR Path, R_Core *Renderer);
static void RenderBillboard(R_MeshConstants *MeshConstants, R_Core *const Renderer, EReservedSrvIndex SrvIndex);

static void SignalAndWait(R_Core *const Renderer);
static void RenderPrimitives(S_Scene *Scene, R_Core *const Renderer, R_Camera *const Camera);

/****************************************************
Public functions
*****************************************************/

void
R_Init(R_Core *const Renderer, HWND hWnd)
{
	Renderer->hWnd = hWnd;
	Renderer->AspectRatio = (FLOAT)(Renderer->Width) / (Renderer->Height);
	Renderer->Viewport = (D3D12_VIEWPORT){
	  .TopLeftX = 0.0f, .TopLeftY = 0.0f, .Width = (FLOAT)(Renderer->Width), .Height = (FLOAT)(Renderer->Height), .MinDepth = 0.0f, .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(Renderer->Width), (LONG)(Renderer->Height)};
	Renderer->State = ERS_GLTF;

	/* D3D12 setup */

	INT bIsDebugFactory = 0;
#if defined(_DEBUG)
	ID3D12Debug1 *DebugController = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void **)&DebugController))) {
		ID3D12Debug1_EnableDebugLayer(DebugController);
		// ID3D12Debug1_SetEnableGPUBasedValidation(debug_controller, 1);
		bIsDebugFactory |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1_Release(DebugController);
	}
#endif

	IDXGIFactory2 *Factory = NULL;
	HRESULT hr = CreateDXGIFactory2(bIsDebugFactory, &IID_IDXGIFactory2, &Factory);
	ExitIfFailed(hr);

	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &Renderer->Device);
	ExitIfFailed(hr);

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {
	  .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	  .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	  .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateCommandQueue(Renderer->Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &Renderer->CommandQueue);
	ExitIfFailed(hr);

	Renderer->FenceValue = 0;
	hr = ID3D12Device_CreateFence(Renderer->Device, Renderer->FenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &Renderer->Fence);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandAllocator(Renderer->Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
											 &Renderer->CommandAllocator);
	ExitIfFailed(hr);

	D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  .NumDescriptors = MAX_TEXTURES,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &SrvHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->TexturesHeap);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->CommandAllocator,
										Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1, &Renderer->CommandList);
	ExitIfFailed(hr);

	Renderer->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (Renderer->FenceEvent == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		ExitIfFailed(hr);
	}

	D3D12_DESCRIPTOR_HEAP_DESC RtvDescHeapDesc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	  .NumDescriptors = FRAME_COUNT,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &RtvDescHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->RtvDescriptorHeap);
	ExitIfFailed(hr);

	Renderer->RtvDescIncrement = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {
	  .Width = Renderer->Width,
	  .Height = Renderer->Height,
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .Stereo = 0,
	  .SampleDesc.Count = 1,
	  .SampleDesc.Quality = 0,
	  .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
	  .BufferCount = 2,
	  .Scaling = DXGI_SCALING_STRETCH,
	  .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
	  .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
	  .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
	};
	hr = IDXGIFactory2_CreateSwapChainForHwnd(Factory, Renderer->CommandQueue, Renderer->hWnd, &SwapChainDesc, NULL, NULL, &Renderer->SwapChain);
	ExitIfFailed(hr);

	SetRtvBuffers(Renderer, FRAME_COUNT);
	CreateSceneResources(Renderer);
	CreateDepthStencilBuffer(Renderer);
	CreateShaders(Renderer);
	IDXGIFactory2_Release(Factory);
}

void
R_Draw(R_Core *const Renderer, S_Scene *Scene, R_Camera *const Camera)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignPBR);
	ID3D12GraphicsCommandList_RSSetViewports(Renderer->CommandList, 1, &Renderer->Viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(Renderer->CommandList, 1, &Renderer->ScissorRect);

	D3D12_RESOURCE_BARRIER ResourceBarrier = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition.pResource = Renderer->RtvBuffers[Renderer->RtvIndex],
	  .Transition.Subresource = 0,
	  .Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
	  .Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
	  .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
	};

	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &ResourceBarrier);
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCPUHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->DepthStencilHeap, &DepthStencilCPUHandle);
	ID3D12GraphicsCommandList_OMSetRenderTargets(Renderer->CommandList, 1, &Renderer->RtvHandles[Renderer->RtvIndex], FALSE, &DepthStencilCPUHandle);
	ID3D12GraphicsCommandList_ClearRenderTargetView(Renderer->CommandList, Renderer->RtvHandles[Renderer->RtvIndex], CLEAR_COLOR, 0, NULL);
	ID3D12GraphicsCommandList_ClearDepthStencilView(Renderer->CommandList, DepthStencilCPUHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap *Heaps[] = {Renderer->TexturesHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, 1, Heaps);

	Renderer->MeshDataOffset = 0;
	Renderer->SceneDataOffset = 0;
	RenderPrimitives(Scene, Renderer, Camera);
	XMVECTOR LightPos = XMLoadFloat3(&Scene->Data.LightPosition);
	R_MeshConstants LightMeshData = {
	  .View = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection),
	  .Proj = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f),
	  .Model = XMMatrixTranslationFromVector(XM_REF_1V(LightPos)),
	};
	RenderBillboard(&LightMeshData, Renderer, ERSI_BILLBOARD_LAMP);
	UI_Draw(Renderer->CommandList);

	// Bring the rtv resource back to present state
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = Renderer->RtvBuffers[Renderer->RtvIndex];
	ResourceBarrier.Transition.Subresource = 0;
	ResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	ResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &ResourceBarrier);

	R_ExecuteCommands(Renderer);

	HRESULT hr = IDXGISwapChain2_Present(Renderer->SwapChain, 1, 0);
	ExitIfFailed(hr);
	Renderer->RtvIndex = (Renderer->RtvIndex + 1) % FRAME_COUNT;
}

void
R_ExecuteCommands(R_Core *const Renderer)
{
	ID3D12GraphicsCommandList_Close(Renderer->CommandList);
	ID3D12CommandList *CmdLists[] = {(ID3D12CommandList *)Renderer->CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(Renderer->CommandQueue, 1, CmdLists);
	SignalAndWait(Renderer);
	ID3D12CommandAllocator_Reset(Renderer->CommandAllocator);
	ID3D12GraphicsCommandList_Reset(Renderer->CommandList, Renderer->CommandAllocator, Renderer->PipelineState[Renderer->State]);
}

void
R_SwapchainResize(R_Core *const Renderer, INT Width, INT Height)
{
	for (INT i = 0; i < FRAME_COUNT; ++i) {
		SignalAndWait(Renderer);
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}

	HRESULT hr = IDXGISwapChain1_ResizeBuffers(Renderer->SwapChain, 2, Width, Height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, 0, &IID_ID3D12Resource, &Renderer->RtvBuffers[0]);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, 1, &IID_ID3D12Resource, &Renderer->RtvBuffers[1]);
	ExitIfFailed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->RtvDescriptorHeap, &DescriptorHandle);
	ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[0], NULL, DescriptorHandle);
	Renderer->RtvHandles[0] = DescriptorHandle;
	DescriptorHandle.ptr += Renderer->RtvDescIncrement;
	ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[1], NULL, DescriptorHandle);
	Renderer->RtvHandles[1] = DescriptorHandle;
	Renderer->RtvIndex = 0;

	Renderer->Width = Width;
	Renderer->Height = Height;
	Renderer->AspectRatio = (FLOAT)Width / (FLOAT)Height;
	Renderer->Viewport =
		(D3D12_VIEWPORT){.TopLeftX = 0.0f, .TopLeftY = 0.0f, .Width = (FLOAT)Width, .Height = (FLOAT)Height, .MinDepth = 0.0f, .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)Width, (LONG)Height};

	if (Renderer->DepthStencil) {
		ID3D12Resource_Release(Renderer->DepthStencil);
	}

	CreateDepthStencilBuffer(Renderer);
}

GPUTexture
R_UploadTexture(R_Core *Renderer, R_Texture *Source)
{
	ptrdiff_t Index = shgeti(Renderer->Textures, Source->Name);
	if (Index != -1) {
		return Renderer->Textures[Index].Texture;
	}

	uint32_t SlotIndex = Renderer->TexturesCount++;
	GPUTexture NewTex = {
	  .GpuTexture = CommandCreateTextureGPU(Renderer, Source),
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

	TextureLookup Lookup = {.key = _strdup(Source->Name), .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	return NewTex;
}

void
R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex)
{
	char PathUTF8[MAX_PATH * 4];
	WideCharToMultiByte(CP_UTF8, 0, Path, -1, PathUTF8, (INT)sizeof(PathUTF8), NULL, NULL);
	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){
	  .Height = H,
	  .Width = W,
	  .Pixels = Pixels,
	};

	GPUTexture NewTex = {0};
	NewTex.GpuTexture = CommandCreateTextureGPU(Renderer, &Source);
	UI_SetTextureInNkHeap(nkSlotIndex, NewTex.GpuTexture);

	TextureLookup Lookup = {.key = Path, .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	stbi_image_free(Pixels);
}

void
CreateCustomTexture(PCWSTR Path, R_Core *Renderer)
{
	char PathUTF8[MAX_PATH * 4];
	WideCharToMultiByte(CP_UTF8, 0, Path, -1, PathUTF8, (INT)sizeof(PathUTF8), NULL, NULL);
	INT W, H;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, NULL, 4);
	R_Texture Source = (R_Texture){.Height = H, .Width = W, .Pixels = Pixels, .Name = "teste"};
	GPUTexture NewTex = R_UploadTexture(Renderer, &Source);
	Renderer->TexturesCount = 1;
	stbi_image_free(Pixels);
}

void
R_Destroy(R_Core *Renderer)
{
	UI_Destroy();
	IDXGISwapChain1_Release(Renderer->SwapChain);
	ID3D12DescriptorHeap_Release(Renderer->RtvDescriptorHeap);
	ID3D12Device_Release(Renderer->TexturesHeap);
	for (INT i = 0; i < FRAME_COUNT; ++i) {
		SignalAndWait(Renderer);
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}
	ID3D12Device_Release(Renderer->Device);
	ID3D12GraphicsCommandList_Release(Renderer->DepthStencil);
	ID3D12GraphicsCommandList_Release(Renderer->DepthStencilHeap);
	ID3D12CommandAllocator_Release(Renderer->CommandAllocator);
	ID3D12GraphicsCommandList_Release(Renderer->CommandList);
	ID3D12CommandQueue_Release(Renderer->CommandQueue);
	ID3D12Fence_Release(Renderer->Fence);
	for (ERenderState State = ERS_GLTF; State < ERS_N_RENDER_STATES; ++State) {
		ID3D12PipelineState_Release(Renderer->PipelineState[State]);
	}
	ID3D12Resource_Release(Renderer->VertexBufferDefault);
	ID3D12Resource_Release(Renderer->IndexBufferDefault);
	ID3D12Resource_Release(Renderer->VertexBufferUpload);
	ID3D12Resource_Release(Renderer->SceneDataUploadBuffer);
	CloseHandle(Renderer->FenceEvent);

	for (INT i = 0; i < hmlen(Renderer->Textures); ++i) {
		GPUTexture *Value = &Renderer->Textures[i].Texture;
		ID3D12Resource_Release(Value->GpuTexture);
	}

	ID3D12Resource_Release(Renderer->MeshDataUploadBuffer);
	ID3D12Resource_Release(Renderer->TextureUploadBuffer.Buffer);
}

/****************************************************
	Implementation of private functions
*****************************************************/

static void
SignalAndWait(R_Core *const Renderer)
{
	HRESULT hr = ID3D12CommandQueue_Signal(Renderer->CommandQueue, Renderer->Fence, ++Renderer->FenceValue);
	ExitIfFailed(hr);
	ID3D12Fence_SetEventOnCompletion(Renderer->Fence, Renderer->FenceValue, Renderer->FenceEvent);
	WaitForSingleObject(Renderer->FenceEvent, INFINITE);
}

ID3D12Resource *
CommandCreateTextureGPU(R_Core *Renderer, R_Texture *Source)
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
	UINT64 Offset = SuballocateTextureUpload(Renderer, TotalUploadSize);
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

void
UpdateResourceData(ID3D12Resource *Resource, const void *Data, size_t DataSize, UINT64 Offset)
{
	UINT8 *Begin = NULL;
	const D3D12_RANGE ReadRange = {0, 0};
	HRESULT hr = ID3D12Resource_Map(Resource, 0, &ReadRange, &Begin);
	ExitIfFailed(hr);
	memcpy(Begin + Offset, Data, DataSize);
	ID3D12Resource_Unmap(Resource, 0, NULL);
}

void
RenderPrimitives(S_Scene *Scene, R_Core *const Renderer, R_Camera *const Camera)
{
	UINT8 *MeshDataCpuAddress = Renderer->MeshDataUploadBufferCpuAddress;
	D3D12_GPU_VIRTUAL_ADDRESS MeshDataGpuAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->MeshDataUploadBuffer);

	R_MeshConstants MeshData;
	MeshData.View = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection);
	MeshData.Proj = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f);

	UpdateResourceData(Renderer->SceneDataUploadBuffer, &Scene->Data, sizeof(R_SceneData), Renderer->SceneDataOffset);
	D3D12_GPU_VIRTUAL_ADDRESS SceneDataGpuBaseAddress =
		ID3D12Resource_GetGPUVirtualAddress(Renderer->SceneDataUploadBuffer) + Renderer->SceneDataOffset;
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 2, SceneDataGpuBaseAddress);
	Renderer->SceneDataOffset += (sizeof(R_SceneData) + 255) & ~255;

	D3D12_GPU_DESCRIPTOR_HANDLE TexturesHeapStart;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &TexturesHeapStart);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 3, TexturesHeapStart);

	for (INT ModelIdx = 0; ModelIdx < Scene->ModelsCount; ++ModelIdx) {
		R_Model *Model = &Scene->Models[ModelIdx];
		for (INT MeshIdx = 0; MeshIdx < Model->MeshesCount; ++MeshIdx) {
			R_Mesh *Mesh = &Model->Meshes[MeshIdx];
			MeshData.Model = XMLoadFloat4x4(&Mesh->ModelMatrix);
			MeshData.Normal = R_NormalMatrix(Mesh->ModelMatrix);
			memcpy(MeshDataCpuAddress + Renderer->MeshDataOffset, &MeshData, sizeof(R_MeshConstants));
			ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, MeshDataGpuAddress + Renderer->MeshDataOffset);
			Renderer->MeshDataOffset += (sizeof(R_MeshConstants) + 255) & ~255;

			for (INT PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
				R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];
				ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(Renderer->CommandList, 1, NUM_32BITS_PBR_VALUES, &Primitive->cb, 0);
				ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &Primitive->VertexBufferView);
				ID3D12GraphicsCommandList_IASetIndexBuffer(Renderer->CommandList, &Primitive->IndexBufferView);
				ID3D12GraphicsCommandList_DrawIndexedInstanced(Renderer->CommandList, Primitive->IndexCount, 1, 0, 0, 0);
			}
		}
	}
}

void
CreateDepthStencilBuffer(R_Core *const Renderer)
{
	D3D12_DESCRIPTOR_HEAP_DESC DepthStencilHeapDesc = {
	  .NumDescriptors = 1, .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
	HRESULT hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &DepthStencilHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->DepthStencilHeap);
	ExitIfFailed(hr);

	D3D12_RESOURCE_DESC TextureDesc = CD3DX12_TEX2D(DXGI_FORMAT_D32_FLOAT, Renderer->Width, Renderer->Height, 1, 0, 1, 0,
													D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
	D3D12_CLEAR_VALUE DepthOptimizedClearValue = {.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil.Depth = 1.0f};
	D3D12_HEAP_PROPERTIES DefaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeap, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
										 &DepthOptimizedClearValue, &IID_ID3D12Resource, &Renderer->DepthStencil);

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->DepthStencilHeap, &DepthStencilHandle);
	ID3D12Device_CreateDepthStencilView(Renderer->Device, Renderer->DepthStencil, NULL, DepthStencilHandle);
}

UINT64
SuballocateTextureUpload(R_Core *Renderer, UINT64 Size)
{
	UINT64 AlignedOffset =
		(Renderer->TextureUploadBuffer.CurrentOffset + (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

	if (AlignedOffset + Size > Renderer->TextureUploadBuffer.Size) {
		// What do I do in this case?
	}

	Renderer->TextureUploadBuffer.CurrentOffset = AlignedOffset + Size;
	return AlignedOffset;
}

void
SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers)
{
	D3D12_CPU_DESCRIPTOR_HANDLE RtvDescriptorHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->RtvDescriptorHeap, &RtvDescriptorHandle);
	for (int i = 0; i < NumBuffers; ++i) {
		HRESULT hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, i, &IID_ID3D12Resource, &Renderer->RtvBuffers[i]);
		ExitIfFailed(hr);
		ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[i], NULL, RtvDescriptorHandle);
		Renderer->RtvHandles[i] = RtvDescriptorHandle;
		RtvDescriptorHandle.ptr += Renderer->RtvDescIncrement;
	}
}

void
CreateSceneResources(R_Core *const Renderer)
{
	HRESULT hr;
	D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(MEGABYTES(128), D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES UploadHeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_HEAP_PROPERTIES DefaultHeapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->VertexBufferUpload);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON,
											  NULL, &IID_ID3D12Resource, &Renderer->VertexBufferDefault);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON,
											  NULL, &IID_ID3D12Resource, &Renderer->IndexBufferDefault);
	ExitIfFailed(hr);

	BufferDesc.Width = MEGABYTES(1);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->MeshDataUploadBuffer);
	ExitIfFailed(hr);
	ID3D12Resource_Map(Renderer->MeshDataUploadBuffer, 0, NULL, &Renderer->MeshDataUploadBufferCpuAddress);

	BufferDesc.Width = KILOBYTES(1);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->SceneDataUploadBuffer);
	ExitIfFailed(hr);

	D3D12_HEAP_PROPERTIES HeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	Renderer->TextureUploadBuffer.Size = MEGABYTES(128);
	Renderer->TextureUploadBuffer.CurrentOffset = 0;
	D3D12_RESOURCE_DESC TextureBufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(Renderer->TextureUploadBuffer.Size, D3D12_RESOURCE_FLAG_NONE, 0);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &TextureBufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->TextureUploadBuffer.Buffer);
	ExitIfFailed(hr);
	D3D12_RANGE Range = {0, 0};
	ID3D12Resource_Map(Renderer->TextureUploadBuffer.Buffer, 0, &Range, &Renderer->TextureUploadBuffer.BaseMappedPtr);

	WCHAR LampImagePath[512];
	Win32FullPath(L"/assets/images/lamp.png", LampImagePath, _countof(LampImagePath));
	CreateCustomTexture(LampImagePath, Renderer);
}

void
CreateShaders(R_Core *const Renderer)
{
	R_CreatePBRPipelineState(Renderer);
	R_CreateBillboardPipelineState(Renderer);
}

void
RenderBillboard(R_MeshConstants *MeshConstants, R_Core *const Renderer, EReservedSrvIndex SrvIndex)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignBillboard);
	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[ERS_BILLBOARD]);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	UpdateResourceData(Renderer->MeshDataUploadBuffer, MeshConstants, sizeof(R_MeshConstants), Renderer->MeshDataOffset);

	UINT IncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_GPU_DESCRIPTOR_HANDLE LampHandle;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &LampHandle);
	LampHandle.ptr += (UINT64)SrvIndex * IncrementSize;

	D3D12_GPU_VIRTUAL_ADDRESS MeshDataGpuAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->MeshDataUploadBuffer);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, MeshDataGpuAddress + Renderer->MeshDataOffset);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 1, LampHandle);

	struct BillboardVertex {
		XMFLOAT3 Position;
		XMFLOAT2 UV;
	} BillboardVertices[] = {
	  {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
	  {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	  {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
	  {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	};
	UpdateResourceData(Renderer->SceneDataUploadBuffer, &BillboardVertices, sizeof(BillboardVertices), Renderer->SceneDataOffset);

	D3D12_VERTEX_BUFFER_VIEW VBV = {
	  .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->SceneDataUploadBuffer) + Renderer->SceneDataOffset,
	  .SizeInBytes = sizeof(BillboardVertices),
	  .StrideInBytes = sizeof(struct BillboardVertex),
	};
	ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &VBV);
	ID3D12GraphicsCommandList_DrawInstanced(Renderer->CommandList, 4, 1, 0, 0);

	Renderer->SceneDataOffset += (sizeof(BillboardVertices) + 255) & ~255;
	Renderer->MeshDataOffset += (sizeof(R_MeshConstants) + 255) & ~255;
}