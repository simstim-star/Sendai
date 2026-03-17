#include "../core/pch.h"

#include "renderer.h"
#include "render_types.h"
#include "shader.h"

#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../ui/ui.h"
#include "../win32/win_path.h"
#include "../core/camera.h"

#define STB_DS_IMPLEMENTATION
#include "../../deps/stb_ds.h"

#include "../../deps/stb_image.h"

static const FLOAT CLEAR_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void SignalAndWait(R_World *const Renderer);
static void RenderPrimitives(S_Scene *Scene, R_World *const Renderer, R_Camera *const Camera);
static void CreateDepthStencilBuffer(R_World *const Renderer);
static ID3D12Resource *CreateGPUTexture(R_World *Renderer, R_Texture *Source);
static UINT64 SuballocateTextureUpload(R_World *Renderer, UINT64 Size);
static void UpdateResourceData(ID3D12Resource *Resource, const void *Data, size_t DataSize);

/****************************************************
Public functions
*****************************************************/

void
R_Init(R_World *const Renderer, HWND hWnd)
{
	Renderer->hWnd = hWnd;
	Renderer->AspectRatio = (FLOAT)(Renderer->Width) / (Renderer->Height);
	Renderer->Viewport = (D3D12_VIEWPORT){
	  .TopLeftX = 0.0f, .TopLeftY = 0.0f, .Width = (FLOAT)(Renderer->Width), .Height = (FLOAT)(Renderer->Height), .MinDepth = 0.0f, .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(Renderer->Width), (LONG)(Renderer->Height)};
	Renderer->State = RENDER_STATE_GLTF;

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
	HRESULT hr = CreateDXGIFactory2(bIsDebugFactory, &IID_IDXGIFactory2, (void **)&Factory);
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
	  .NumDescriptors = 256,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  .NodeMask = 0,
	};

	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &SrvHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->SrvHeap);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->CommandAllocator,
										Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1, &Renderer->CommandList);
	ExitIfFailed(hr);

	const D3D12_HEAP_PROPERTIES HeapPropertyUpload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};

	UINT PBRBufferSize = GET_ALIGNED_SIZE(R_PBRConstantBuffer, CB_ALIGNMENT);
	const D3D12_RESOURCE_DESC PBRDesc = CD3DX12_RESOURCE_DESC_BUFFER(PBRBufferSize, D3D12_RESOURCE_FLAG_NONE, 0);

	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapPropertyUpload, D3D12_HEAP_FLAG_NONE, &PBRDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->MaterialBuffer);
	ExitIfFailed(hr);

	Renderer->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (Renderer->FenceEvent == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		ExitIfFailed(hr);
	}

	D3D12_DESCRIPTOR_HEAP_DESC RtvDescHeapDesc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	  .NumDescriptors = 256,
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
	hr = IDXGIFactory2_CreateSwapChainForHwnd(Factory, (IUnknown *)Renderer->CommandQueue, Renderer->hWnd, &SwapChainDesc, NULL, NULL,
											  &Renderer->SwapChain);
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

	D3D12_DESCRIPTOR_HEAP_DESC DepthStencilHeapDesc = {
	  .NumDescriptors = 1, .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &DepthStencilHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->DepthStencilHeap);
	ExitIfFailed(hr);

	const D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(MEGABYTES(128), D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES UploadHeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
										 NULL, &IID_ID3D12Resource, &Renderer->UploadBuffer);

	D3D12_HEAP_PROPERTIES DefaultHeapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, NULL,
										 &IID_ID3D12Resource, &Renderer->VertexBuffer);
	ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, NULL,
										 &IID_ID3D12Resource, &Renderer->IndexBuffer);

	D3D12_HEAP_PROPERTIES HeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_RESOURCE_DESC DynamicBufferDesc = {
	  .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
	  .Width = MEGABYTES(1),
	  .Height = 1,
	  .DepthOrArraySize = 1,
	  .MipLevels = 1,
	  .Format = DXGI_FORMAT_UNKNOWN,
	  .SampleDesc.Count = 1,
	  .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};
	ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &DynamicBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
										 NULL, &IID_ID3D12Resource, &Renderer->VertexDataUploadBuffer);
	ID3D12Resource_Map(Renderer->VertexDataUploadBuffer, 0, NULL, &Renderer->VertexDataUploadBufferCpuAddress);

	DynamicBufferDesc.Width = KILOBYTES(1);
	ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &DynamicBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
										 NULL, &IID_ID3D12Resource, &Renderer->LightDataUploadBuffer);
	
	CreateDepthStencilBuffer(Renderer);

	Renderer->TextureUploadBuffer.Size = MEGABYTES(128);
	Renderer->TextureUploadBuffer.CurrentOffset = 0;
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC_BUFFER(Renderer->TextureUploadBuffer.Size, D3D12_RESOURCE_FLAG_NONE, 0);
	ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
										 &IID_ID3D12Resource, &Renderer->TextureUploadBuffer.Resource);
	D3D12_RANGE Range = {0, 0}; 
	ID3D12Resource_Map(Renderer->TextureUploadBuffer.Resource, 0, &Range, (void **)&Renderer->TextureUploadBuffer.BaseMappedPtr);

	IDXGIFactory2_Release(Factory);
}

void
R_Draw(R_World *const Renderer, S_Scene *Scene, R_Camera *const Camera)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSign);
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
	ID3D12DescriptorHeap *Heaps[] = {Renderer->SrvHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, 1, Heaps);

	RenderPrimitives(Scene, Renderer, Camera);
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
	Renderer->RtvIndex = (Renderer->RtvIndex + 1) % FRAME_COUNT;
	if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
		MessageBox(NULL, L"D3D12 device is lost or removed!", L"Error", 0);
		return;
	}
	ExitIfFailed(hr);
}

void
R_ExecuteCommands(R_World *const Renderer)
{
	ID3D12GraphicsCommandList_Close(Renderer->CommandList);
	ID3D12CommandList *CmdLists[] = {(ID3D12CommandList *)Renderer->CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(Renderer->CommandQueue, 1, CmdLists);
	SignalAndWait(Renderer);
	ID3D12CommandAllocator_Reset(Renderer->CommandAllocator);
	ID3D12GraphicsCommandList_Reset(Renderer->CommandList, Renderer->CommandAllocator, Renderer->PipelineState[Renderer->State]);
}

void
R_SwapchainResize(R_World *const Renderer, INT Width, INT Height)
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
R_UploadTexture(R_World *Renderer, R_Texture *Source, UINT SlotIndex)
{
	GPUTexture NewTex = {0};

	ptrdiff_t Index = shgeti(Renderer->Textures, Source->Name);
	if (Index != -1) {
		NewTex.GpuTexture = Renderer->Textures[Index].Texture.GpuTexture;
	} else {
		NewTex.GpuTexture = CreateGPUTexture(Renderer, Source);
		TextureLookup Lookup = {.key = _strdup(Source->Name), .Texture = NewTex};
		shputs(Renderer->Textures, Lookup);
	}

	UINT IncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE CpuDescHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->SrvHeap, &CpuDescHandle);
	CpuDescHandle.ptr += (SIZE_T)SlotIndex * IncrementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = 1,
	};
	ID3D12Device_CreateShaderResourceView(Renderer->Device, NewTex.GpuTexture, &SrvDesc, CpuDescHandle);

	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->SrvHeap, &NewTex.SrvHandle);
	NewTex.SrvHandle.ptr += (UINT64)SlotIndex * IncrementSize;

	return NewTex;
}

void
R_CreateUITexture(PCWSTR Path, R_World *Renderer, UINT nkSlotIndex)
{
	char PathUTF8[MAX_PATH * 4];
	WideCharToMultiByte(CP_UTF8, 0, Path, -1, PathUTF8, (INT)sizeof(PathUTF8), NULL, NULL);
	INT W, H, Ch;
	UINT8 *Pixels = stbi_load(PathUTF8, &W, &H, &Ch, 4);
	size_t Size = (size_t)(W) * (size_t)(H) * 4;
	R_Texture Source = (R_Texture){
	  .Height = H,
	  .Width = W,
	  .Pixels = Pixels,
	};

	GPUTexture NewTex = {0};
	NewTex.GpuTexture = CreateGPUTexture(Renderer, &Source);
	UI_SetTextureInNkHeap(nkSlotIndex, NewTex.GpuTexture);
	TextureLookup Lookup = {.key = "ui_toolbar_wireframe", .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);
	stbi_image_free(Pixels);
}

void
R_UpdateResource(ID3D12Resource *Resource, void *Data, size_t DataSize)
{
	UINT8 *Begin = NULL;
	const D3D12_RANGE ReadRange = {0, 0};
	HRESULT hr = ID3D12Resource_Map(Resource, 0, &ReadRange, (void **)&Begin);
	ExitIfFailed(hr);
	memcpy(Begin, Data, DataSize);
	ID3D12Resource_Unmap(Resource, 0, NULL);
}

void
R_Destroy(R_World *Renderer)
{
	UI_Destroy();
	IDXGISwapChain1_Release(Renderer->SwapChain);
	ID3D12DescriptorHeap_Release(Renderer->RtvDescriptorHeap);
	ID3D12Device_Release(Renderer->SrvHeap);
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
	for (RENDER_STATE State = RENDER_STATE_GLTF; State < N_RENDER_STATES; ++State) {
		ID3D12PipelineState_Release(Renderer->PipelineState[State]);
	}
	ID3D12Resource_Release(Renderer->VertexBuffer);
	ID3D12Resource_Release(Renderer->IndexBuffer);
	ID3D12Resource_Release(Renderer->UploadBuffer);
	ID3D12Resource_Release(Renderer->LightDataUploadBuffer);
	CloseHandle(Renderer->FenceEvent);

	for (INT i = 0; i < hmlen(Renderer->Textures); ++i) {
		GPUTexture *Value = &Renderer->Textures[i].Texture;
		ID3D12Resource_Release(Value->GpuTexture);
	}

	ID3D12Resource_Release(Renderer->MaterialBuffer);
	ID3D12Resource_Release(Renderer->VertexDataUploadBuffer);
	ID3D12Resource_Release(Renderer->TextureUploadBuffer.Resource);
}

/****************************************************
	Implementation of private functions
*****************************************************/

static void
SignalAndWait(R_World *const Renderer)
{
	HRESULT hr = ID3D12CommandQueue_Signal(Renderer->CommandQueue, Renderer->Fence, ++Renderer->FenceValue);
	ExitIfFailed(hr);
	ID3D12Fence_SetEventOnCompletion(Renderer->Fence, Renderer->FenceValue, Renderer->FenceEvent);
	WaitForSingleObject(Renderer->FenceEvent, INFINITE);
}

ID3D12Resource *
CreateGPUTexture(R_World *Renderer, R_Texture *Source)
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
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT FootprINT;
	ID3D12Device_GetCopyableFootprints(Renderer->Device, &TexDesc, 0, 1, 0, &FootprINT, &NumRows, &RowSize, &TotalUploadSize);
	UINT64 Offset = SuballocateTextureUpload(Renderer, TotalUploadSize);
	FootprINT.Offset += Offset;
	UINT8 *DestPtr = Renderer->TextureUploadBuffer.BaseMappedPtr + FootprINT.Offset;
	for (UINT i = 0; i < NumRows; ++i) {
		memcpy(DestPtr + i * FootprINT.Footprint.RowPitch, (UINT8 *)Source->Pixels + i * (Source->Width * 4), Source->Width * 4);
	}
	D3D12_TEXTURE_COPY_LOCATION DstLocation = {.pResource = Texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
	D3D12_TEXTURE_COPY_LOCATION SrcLocation = {
	  .pResource = Renderer->TextureUploadBuffer.Resource, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = FootprINT};
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
UpdateResourceData(ID3D12Resource *Resource, const void* Data, size_t DataSize)
{
	UINT8 *Begin = NULL;
	const D3D12_RANGE ReadRange = {0, 0};
	HRESULT hr = ID3D12Resource_Map(Resource, 0, &ReadRange, &Begin);
	ExitIfFailed(hr);
	memcpy(Begin, Data, DataSize);
	ID3D12Resource_Unmap(Resource, 0, NULL);
}

void
RenderPrimitives(S_Scene *Scene, R_World *const Renderer, R_Camera *const Camera)
{
	UINT64 UploadCpuOffset = 0;
	UINT8 *UploadCpuBaseAddress = Renderer->VertexDataUploadBufferCpuAddress;
	D3D12_GPU_VIRTUAL_ADDRESS UploadGpuBaseAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->VertexDataUploadBuffer);

	XMMATRIX MeshData[4];
	MeshData[1] = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection);
	MeshData[2] = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f);

	R_SceneData SceneData = {
		.ViewPosition = Camera->Position, 
		.Light = {
			.LightPosition = {10.0f, 20.0f, 50.0f, 1.0f},
			.AmbientColor = {.1f, .1f, .1f, 1.0f}, 
			.DiffuseColor = {.7f, .85f, .25f, 1.0f}, 
			.SpecularColor = {1.0f, 1.0f, 1.0f, 1.0f}
		},
		.Shininess = 64,
	};
	R_Light Light = (R_Light){.LightPosition = {10.0f, 20.0f, 10.0f, 1.0f}, .DiffuseColor = {1.0f, 1.0f, 1.0f, 1.0f}};
	UpdateResourceData(Renderer->LightDataUploadBuffer, &SceneData, sizeof(R_SceneData));

	D3D12_GPU_VIRTUAL_ADDRESS LightGpuBaseAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->LightDataUploadBuffer);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 2, LightGpuBaseAddress);

	for (INT ModelIdx = 0; ModelIdx < Scene->ModelsCount; ++ModelIdx) {
		R_Model *Model = &Scene->Models[ModelIdx];
		for (INT MeshIdx = 0; MeshIdx < Model->MeshesCount; ++MeshIdx) {
			R_Mesh *Mesh = &Model->Meshes[MeshIdx];
			MeshData[0] = XMLoadFloat4x4(&Mesh->ModelMatrix);
			MeshData[3] = R_NormalMatrix(Mesh->ModelMatrix);
			memcpy(UploadCpuBaseAddress + UploadCpuOffset, MeshData, sizeof(MeshData));
			ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, UploadGpuBaseAddress + UploadCpuOffset);
			UploadCpuOffset += (sizeof(XMMATRIX) + 255) & ~255;
			
			for (INT PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
				R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];
				ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(Renderer->CommandList, 1, NUM_32BITS_PBR_VALUES, &Primitive->cb, 0);
				if (Primitive->AlbedoIndex >= 0) {
					ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 3, Primitive->MaterialDescriptorBase);
				}
				ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &Primitive->VertexBufferView);
				ID3D12GraphicsCommandList_IASetIndexBuffer(Renderer->CommandList, &Primitive->IndexBufferView);
				ID3D12GraphicsCommandList_DrawIndexedInstanced(Renderer->CommandList, Primitive->IndexCount, 1, 0, 0, 0);
			}
		}
	}
}

void
CreateDepthStencilBuffer(R_World *const Renderer)
{
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
SuballocateTextureUpload(R_World *Renderer, UINT64 Size)
{
	UINT64 AlignedOffset =
		(Renderer->TextureUploadBuffer.CurrentOffset + (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

	if (AlignedOffset + Size > Renderer->TextureUploadBuffer.Size) {
		// What do I do in this case?
	}

	Renderer->TextureUploadBuffer.CurrentOffset = AlignedOffset + Size;
	return AlignedOffset;
}