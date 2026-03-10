#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../ui/ui.h"
#include "../win32/win_path.h"

#include "render_types.h"
#include "renderer.h"

#include "../core/camera.h"

#define STB_DS_IMPLEMENTATION
#include "../../deps/stb_ds.h"

static const float CLEAR_COLOR[] = {0.0f, 0.2f, 0.4f, 1.0f};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void SignalAndWait(R_World *const Renderer);
static void UploadTexture(R_World *Renderer, R_Texture *Source, ID3D12Resource **OutTexture, D3D12_GPU_DESCRIPTOR_HANDLE *OutSrv, UINT SrvIndex);
static void RenderPrimitives(SendaiScene *Scene, R_World *const Renderer);
static void CreateDepthStencilBuffer(R_World *const Renderer);

/****************************************************
Public functions
*****************************************************/

void
R_Init(R_World *const Renderer, HWND hWnd)
{
	Renderer->hWnd = hWnd;
	Renderer->AspectRatio = (float)(Renderer->Width) / (Renderer->Height);
	Renderer->Viewport = (D3D12_VIEWPORT){
	  .TopLeftX = 0.0f, .TopLeftY = 0.0f, .Width = (float)(Renderer->Width), .Height = (float)(Renderer->Height), .MinDepth = 0.0f, .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(Renderer->Width), (LONG)(Renderer->Height)};
	Win32CurrPath(Renderer->AssetsPath, _countof(Renderer->AssetsPath));

	/* D3D12 setup */

	int isDebugFactory = 0;
#if defined(_DEBUG)
	ID3D12Debug1 *DebugController = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void **)&DebugController))) {
		ID3D12Debug1_EnableDebugLayer(DebugController);
		// ID3D12Debug1_SetEnableGPUBasedValidation(debug_controller, 1);
		isDebugFactory |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1_Release(DebugController);
	}
#endif

	IDXGIFactory2 *Factory = NULL;
	HRESULT hr = CreateDXGIFactory2(isDebugFactory, &IID_IDXGIFactory2, (void **)&Factory);
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

	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->CommandAllocator, Renderer->PipelineStateScene,
										&IID_ID3D12GraphicsCommandList1, &Renderer->CommandList);
	ExitIfFailed(hr);

	const D3D12_HEAP_PROPERTIES HeapPropertyUpload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};

	UINT TransformBufferSize = GET_ALIGNED_SIZE(R_TransformBuffer, CB_ALIGNMENT);
	const D3D12_RESOURCE_DESC CBDesc = CD3DX12_RESOURCE_DESC_BUFFER(TransformBufferSize, D3D12_RESOURCE_FLAG_NONE, 0);

	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapPropertyUpload, D3D12_HEAP_FLAG_NONE, &CBDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
											  NULL, &IID_ID3D12Resource, &Renderer->TransformBuffer);
	ExitIfFailed(hr);

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

	CreateDepthStencilBuffer(Renderer);

	IDXGIFactory2_Release(Factory);
}

D3D12_GPU_VIRTUAL_ADDRESS
R_UploadStaticData(ID3D12Device *Device, ID3D12GraphicsCommandList *CmdList, UINT BufferSize, void *Data, ID3D12Resource **ppResource)
{
	const D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(BufferSize, D3D12_RESOURCE_FLAG_NONE, 0);
	ID3D12Resource *UploadBuffer;
	D3D12_HEAP_PROPERTIES UploadHeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};

	ID3D12Device_CreateCommittedResource(Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
										 &IID_ID3D12Resource, &UploadBuffer);

	void *MappedToUploadBuffer;
	ID3D12Resource_Map(UploadBuffer, 0, NULL, &MappedToUploadBuffer);
	memcpy(MappedToUploadBuffer, Data, BufferSize);
	ID3D12Resource_Unmap(UploadBuffer, 0, NULL);

	D3D12_HEAP_PROPERTIES DefaultHeapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	ID3D12Device_CreateCommittedResource(Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
										 &IID_ID3D12Resource, ppResource);

	ID3D12Resource *pResource = *ppResource;
	ID3D12GraphicsCommandList_CopyBufferRegion(CmdList, pResource, 0, UploadBuffer, 0, BufferSize);

	D3D12_RESOURCE_BARRIER Barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition.pResource = pResource,
									  .Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
									  .Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
									  .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES};

	ID3D12GraphicsCommandList_ResourceBarrier(CmdList, 1, &Barrier);

	return ID3D12Resource_GetGPUVirtualAddress(pResource);
}

void
R_Update(R_World *const Renderer, R_Camera *const Camera, SendaiScene *Scene)
{
	XMMATRIX View = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection);
	XMMATRIX Proj = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f);
	XMMATRIX MVP = XM_MAT_MULT(View, Proj);
	MVP = XM_MAT_TRANSP(MVP);
	R_UpdateResource(Renderer->TransformBuffer, &MVP, sizeof(XMMATRIX));
}

void
R_Draw(R_World *const Renderer, SendaiScene *Scene)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Scene->RootSign);
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
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0,
																ID3D12Resource_GetGPUVirtualAddress(Renderer->TransformBuffer));
	ID3D12DescriptorHeap *Heaps[] = {Renderer->SrvHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, 1, Heaps);

	RenderPrimitives(Scene, Renderer);

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
	ID3D12GraphicsCommandList_Reset(Renderer->CommandList, Renderer->CommandAllocator, Renderer->PipelineStateScene);
}

void
R_Destroy(R_World *Renderer)
{
	UI_Destroy();
	for (int i = 0; i < FRAME_COUNT; ++i) {
		SignalAndWait(Renderer);
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}
	ID3D12DescriptorHeap_Release(Renderer->RtvDescriptorHeap);
	IDXGISwapChain1_Release(Renderer->SwapChain);
	ID3D12GraphicsCommandList_Release(Renderer->CommandList);
	ID3D12CommandAllocator_Release(Renderer->CommandAllocator);
	ID3D12CommandQueue_Release(Renderer->CommandQueue);
	ID3D12Fence_Release(Renderer->Fence);
	ID3D12PipelineState_Release(Renderer->PipelineStateScene);
	ID3D12Resource_Release(Renderer->TransformBuffer);
	ID3D12Device_Release(Renderer->Device);
	ID3D12Device_Release(Renderer->SrvHeap);
	CloseHandle(Renderer->FenceEvent);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (void **)&debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}

void
R_SwapchainResize(R_World *const Renderer, int Width, int Height)
{
	for (int i = 0; i < FRAME_COUNT; ++i) {
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
	Renderer->AspectRatio = (float)Width / (float)Height;
	Renderer->Viewport =
		(D3D12_VIEWPORT){.TopLeftX = 0.0f, .TopLeftY = 0.0f, .Width = (float)Width, .Height = (float)Height, .MinDepth = 0.0f, .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)Width, (LONG)Height};

	if (Renderer->DepthStencil) {
		ID3D12Resource_Release(Renderer->DepthStencil);
	}

	CreateDepthStencilBuffer(Renderer);
}

D3D12_GPU_DESCRIPTOR_HANDLE
R_UploadTexture(R_World *Renderer, R_Texture *Source, UINT SlotIndex)
{
	ptrdiff_t Index = shgeti(Renderer->Textures, Source->Name);
	if (Index != -1) {
		return Renderer->Textures[Index].Texture.SrvHandle;
	}

	GPUTexture NewTex = {0};
	UploadTexture(Renderer, Source, &NewTex.GpuTexture, &NewTex.SrvHandle, SlotIndex);
	TextureLookup Lookup = {.key = _strdup(Source->Name), .Texture = NewTex};
	shputs(Renderer->Textures, Lookup);

	return NewTex.SrvHandle;
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

void
UploadTexture(R_World *Renderer, R_Texture *Source, ID3D12Resource **OutTexture, D3D12_GPU_DESCRIPTOR_HANDLE *OutSrv, UINT SrvIndex)
{
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
	HRESULT hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapDefault, D3D12_HEAP_FLAG_NONE, &TexDesc, D3D12_RESOURCE_STATE_COPY_DEST,
													  NULL, &IID_ID3D12Resource, OutTexture);
	ExitIfFailed(hr);

	UINT64 UploadSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
	UINT NumRows;
	UINT64 RowSize, TotalBytes;
	ID3D12Device_GetCopyableFootprints(Renderer->Device, &TexDesc, 0, 1, 0, &Footprint, &NumRows, &RowSize, &UploadSize);
	D3D12_RESOURCE_DESC UploadDesc = CD3DX12_RESOURCE_DESC_BUFFER(UploadSize, D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES HeapUpload = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	ID3D12Resource *Upload = NULL;
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapUpload, D3D12_HEAP_FLAG_NONE, &UploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
											  NULL, &IID_ID3D12Resource, &Upload);
	ExitIfFailed(hr);

	UINT8 *Mapped = NULL;
	D3D12_RANGE Range = {0, 0};
	ID3D12Resource_Map(Upload, 0, &Range, (void **)&Mapped);
	for (UINT i = 0; i < NumRows; ++i) {
		memcpy(Mapped + Footprint.Offset + i * Footprint.Footprint.RowPitch, Source->Pixels + i * Source->Width * 4, Source->Width * 4);
	}
	ID3D12Resource_Unmap(Upload, 0, NULL);

	D3D12_TEXTURE_COPY_LOCATION DstLocation = {.pResource = *OutTexture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
	D3D12_TEXTURE_COPY_LOCATION SrcLocation = {.pResource = Upload, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = Footprint};
	ID3D12GraphicsCommandList_CopyTextureRegion(Renderer->CommandList, &DstLocation, 0, 0, 0, &SrcLocation, NULL);
	D3D12_RESOURCE_BARRIER Barrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition = {
										.pResource = *OutTexture,
										.Subresource = 0,
										.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
										.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &Barrier);
	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = 1,
	};

	UINT IncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE CpuDescHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->SrvHeap, &CpuDescHandle);
	CpuDescHandle.ptr += (SIZE_T)SrvIndex * IncrementSize;
	ID3D12Device_CreateShaderResourceView(Renderer->Device, *OutTexture, &SrvDesc, CpuDescHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE GpuDescHandle;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->SrvHeap, &GpuDescHandle);
	GpuDescHandle.ptr += (UINT64)SrvIndex * IncrementSize;
	*OutSrv = GpuDescHandle;

	R_ExecuteCommands(Renderer);
	ID3D12Resource_Release(Upload);
}

void
RenderPrimitives(SendaiScene *Scene, R_World *const Renderer)
{
	for (int ModelIdx = 0; ModelIdx < Scene->ModelsCount; ++ModelIdx) {
		R_Model *Model = &Scene->Models[ModelIdx];
		for (int MeshIdx = 0; MeshIdx < Model->MeshesCount; ++MeshIdx) {
			R_Mesh *Mesh = &Model->Meshes[MeshIdx];
			for (int PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
				R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];

				R_PBRConstantBuffer PBRConstants = {0};
				memcpy(PBRConstants.BaseColorFactor, Primitive->BaseColorFactor, sizeof(float) * 4);
				PBRConstants.UVTransform[0] = Primitive->UVScale[0];
				PBRConstants.UVTransform[1] = Primitive->UVScale[1];
				PBRConstants.UVTransform[2] = Primitive->UVOffset[0];
				PBRConstants.UVTransform[3] = Primitive->UVOffset[1];

				ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(Renderer->CommandList, 1, NUM_32BITS_PBR_VALUES, &PBRConstants, 0);

				if (Primitive->AlbedoIndex >= 0) {
					ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 2, Primitive->MaterialDescriptorBase);
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