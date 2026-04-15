#include "core/pch.h"

#include "renderer.h"
#include "render_types.h"
#include "light.h"
#include "shader.h"
#include "texture.h"

#include "core/camera.h"
#include "core/grid.h"
#include "core/memory.h"
#include "dx_helpers/desc_helpers.h"
#include "error/error.h"
#include "shaders/sendai/shader_defs.h"
#include "ui/ui.h"
#include "win32/win_path.h"

#include "stb_ds.h"
#include "billboard.h"

static const FLOAT CLEAR_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers);
static void CreateSceneResources(R_Core *const Renderer);
void CreateBaseEngineTextures(R_Core *const Renderer);
static void CreateDepthStencilBuffer(R_Core *const Renderer);
static void CreateShaders(R_Core *const Renderer);
static R_SceneData PreprocessSceneData(const S_Scene *const Scene);
static void CreateBaseEngineTextures(R_Core *const Renderer);
static void Draw(R_Core *const Renderer, const R_Camera *const Camera, const S_Scene *const Scene);
static void GetAdapter(IDXGIFactory2 *Factory, R_Core *Renderer);

static void SignalAndWait(R_Core *const Renderer);
static void RenderPrimitives(const S_Scene *const Scene, R_Core *const Renderer, R_MeshConstants *const MeshConstants);

/****************************************************
Public functions
*****************************************************/

void
R_Init(R_Core *const Renderer, HWND hWnd)
{
	Renderer->hWnd = hWnd;
	Renderer->AspectRatio = (FLOAT)(Renderer->Width) / (Renderer->Height);
	Renderer->Viewport = (D3D12_VIEWPORT){.TopLeftX = 0.0f,
										  .TopLeftY = 0.0f,
										  .Width = (FLOAT)(Renderer->Width),
										  .Height = (FLOAT)(Renderer->Height),
										  .MinDepth = 0.0f,
										  .MaxDepth = 1.0f};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(Renderer->Width), (LONG)(Renderer->Height)};
	Renderer->State = ERS_GLTF;
	Renderer->bDrawGrid = TRUE;

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

	GetAdapter(Factory, Renderer);

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

	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->CommandAllocator,
										Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1, &Renderer->CommandList);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandAllocator(Renderer->Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
											 &Renderer->UploadCommandAllocator);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->UploadCommandAllocator,
										Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1, &Renderer->UploadCommandList);
	ExitIfFailed(hr);

	Renderer->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (Renderer->FenceEvent == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		ExitIfFailed(hr);
	}

	D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  .NumDescriptors = PBR_N_TEXTURES_DESCRIPTORS,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &SrvHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->TexturesHeap);
	ExitIfFailed(hr);

	D3D12_DESCRIPTOR_HEAP_DESC RtvDescHeapDesc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	  .NumDescriptors = FRAME_COUNT,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &RtvDescHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->RtvDescriptorHeap);
	ExitIfFailed(hr);

	for (D3D12_DESCRIPTOR_HEAP_TYPE Type = 0; Type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++Type) {
		Renderer->DescriptorHandleIncrementSize[Type] = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, Type);
	}

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

	SetRtvBuffers(Renderer, FRAME_COUNT);
	CreateSceneResources(Renderer);
	CreateDepthStencilBuffer(Renderer);
	CreateShaders(Renderer);
	IDXGIFactory2_Release(Factory);
}

void
R_Draw(R_Core *const Renderer, const S_Scene *const Scene, const R_Camera *const Camera)
{
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
	ID3D12GraphicsCommandList_OMSetRenderTargets(Renderer->CommandList, 1, &Renderer->RtvHandles[Renderer->RtvIndex], FALSE,
												 &DepthStencilCPUHandle);
	ID3D12GraphicsCommandList_ClearRenderTargetView(Renderer->CommandList, Renderer->RtvHandles[Renderer->RtvIndex], CLEAR_COLOR, 0, NULL);
	ID3D12GraphicsCommandList_ClearDepthStencilView(Renderer->CommandList, DepthStencilCPUHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap *Heaps[] = {Renderer->TexturesHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, 1, Heaps);

	Draw(Renderer, Camera, Scene);

	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = Renderer->RtvBuffers[Renderer->RtvIndex];
	ResourceBarrier.Transition.Subresource = 0;
	ResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	ResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &ResourceBarrier);

	R_ExecuteCommands(Renderer, Renderer->CommandList, Renderer->CommandAllocator);

	HRESULT hr = IDXGISwapChain2_Present(Renderer->SwapChain, 1, 0);
	ExitIfFailed(hr);
	Renderer->RtvIndex = (Renderer->RtvIndex + 1) % FRAME_COUNT;
}

void
R_ExecuteCommands(R_Core *const Renderer, ID3D12GraphicsCommandList *CommandList, ID3D12CommandAllocator *CommandAllocator)
{
	HRESULT hr = ID3D12GraphicsCommandList_Close(CommandList);
	ExitIfFailed(hr);
	ID3D12CommandList *CmdLists[] = {(ID3D12CommandList *)CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(Renderer->CommandQueue, 1, CmdLists);
	SignalAndWait(Renderer);
	ID3D12CommandAllocator_Reset(CommandAllocator);
	ID3D12GraphicsCommandList_Reset(CommandList, CommandAllocator, Renderer->PipelineState[Renderer->State]);
}

void
R_SwapchainResize(R_Core *const Renderer, INT Width, INT Height)
{
	for (INT i = 0; i < FRAME_COUNT; ++i) {
		SignalAndWait(Renderer);
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}

	HRESULT hr =
		IDXGISwapChain1_ResizeBuffers(Renderer->SwapChain, 2, Width, Height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, 0, &IID_ID3D12Resource, &Renderer->RtvBuffers[0]);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, 1, &IID_ID3D12Resource, &Renderer->RtvBuffers[1]);
	ExitIfFailed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->RtvDescriptorHeap, &DescriptorHandle);
	ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[0], NULL, DescriptorHandle);
	Renderer->RtvHandles[0] = DescriptorHandle;
	DescriptorHandle.ptr += Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
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
		ID3D12DescriptorHeap_Release(Renderer->DepthStencilHeap);
	}

	CreateDepthStencilBuffer(Renderer);
}

void
R_Destroy(R_Core *Renderer)
{
	UI_Destroy();

	for (ERenderState State = ERS_GLTF; State < ERS_N_RENDER_STATES; ++State) {
		ID3D12PipelineState_Release(Renderer->PipelineState[State]);
	}

	ID3D12Resource_Release(Renderer->DepthStencil);
	ID3D12Resource_Release(Renderer->VertexBufferDefault);
	ID3D12Resource_Release(Renderer->IndexBufferDefault);
	ID3D12Resource_Release(Renderer->UploadBuffer);
	ID3D12Resource_Release(Renderer->SceneDataUploadBuffer);
	IDXGIAdapter3_Release(Renderer->Adapter);

	for (INT i = 0; i < hmlen(Renderer->Textures); ++i) {
		ID3D12Resource_Release(Renderer->Textures[i].Texture.GpuTexture);
	}

	for (INT i = 0; i < FRAME_COUNT; ++i) {
		SignalAndWait(Renderer);
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}
	CloseHandle(Renderer->FenceEvent);

	ID3D12DescriptorHeap_Release(Renderer->RtvDescriptorHeap);
	ID3D12DescriptorHeap_Release(Renderer->TexturesHeap);
	ID3D12DescriptorHeap_Release(Renderer->DepthStencilHeap);

	ID3D12Resource_Release(Renderer->MeshDataUploadBuffer);
	ID3D12Resource_Release(Renderer->TextureUploadBuffer.Buffer);

	ID3D12RootSignature_Release(Renderer->RootSignPBR);
	ID3D12RootSignature_Release(Renderer->RootSignBillboard);
	ID3D12RootSignature_Release(Renderer->RootSignGrid);

	ID3D12CommandAllocator_Release(Renderer->CommandAllocator);
	ID3D12GraphicsCommandList_Release(Renderer->CommandList);
	ID3D12CommandQueue_Release(Renderer->CommandQueue);
	ID3D12Fence_Release(Renderer->Fence);
	IDXGISwapChain1_Release(Renderer->SwapChain);
	ID3D12Device_Release(Renderer->Device);

	shfree(Renderer->Textures);
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

void
RenderPrimitives(const S_Scene *const Scene, R_Core *const Renderer, R_MeshConstants *const MeshConstants)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignPBR);
	UINT8 *MeshDataCpuAddress = Renderer->MeshDataUploadBufferCpuAddress;

	R_SceneData SceneData = PreprocessSceneData(Scene);
	memcpy(Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset, &SceneData, sizeof(R_SceneData));
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 2,
																M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset));
	Renderer->SceneDataOffset += CB_ALIGN(R_SceneData);

	D3D12_GPU_DESCRIPTOR_HANDLE TexturesHeapStart;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &TexturesHeapStart);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 3, TexturesHeapStart);

	for (size_t ModelIndex = 0; ModelIndex < Scene->ModelsCount; ++ModelIndex) {
		R_Model *Model = &Scene->Models[ModelIndex];
		if (!Model->Visible) {
			continue;
		}
		XMMATRIX T = XMMatrixTranslation(Model->Position.x, Model->Position.y, Model->Position.z);
		XMMATRIX R = XMMatrixRotationRollPitchYaw(Model->Rotation.x, Model->Rotation.y, Model->Rotation.z);
		XMMATRIX S = XMMatrixScaling(Model->Scale.x, Model->Scale.y, Model->Scale.z);
		XMMATRIX M = XM_MAT_MULT(S, R);
		M = XM_MAT_MULT(M, T);
		for (size_t NodeIndex = 0; NodeIndex < Model->NodesCount; ++NodeIndex) {
			R_Node *Node = &Model->Nodes[NodeIndex];

			MeshConstants->MVP.Model = XMLoadFloat4x4(&Node->ModelMatrix);
			MeshConstants->MVP.Model = XM_MAT_MULT(MeshConstants->MVP.Model, M);
			XMFLOAT4X4 ModelXMFLOAT;
			XM_STORE_FLOAT4X4(&ModelXMFLOAT, MeshConstants->MVP.Model);
			MeshConstants->Normal = R_NormalMatrix(&ModelXMFLOAT);

			memcpy(MeshDataCpuAddress + Renderer->MeshDataOffset, MeshConstants, sizeof(R_MeshConstants));
			ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0,
																		M_GpuAddress(Renderer->MeshDataUploadBuffer, Renderer->MeshDataOffset));
			Renderer->MeshDataOffset += CB_ALIGN(R_MeshConstants);

			if (Node->Mesh != NULL) {
				R_Mesh *Mesh = Node->Mesh;
				for (INT PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
					R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];
					ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(Renderer->CommandList, 1, NUM_32BITS_PBR_VALUES,
																			&Primitive->ConstantBuffer, 0);
					ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &Primitive->VertexBufferView);
					ID3D12GraphicsCommandList_IASetIndexBuffer(Renderer->CommandList, &Primitive->IndexBufferView);
					ID3D12GraphicsCommandList_DrawIndexedInstanced(Renderer->CommandList, Primitive->IndexCount, 1, 0, 0, 0);
				}
			}
		}
	}
}

void
CreateDepthStencilBuffer(R_Core *const Renderer)
{
	D3D12_DESCRIPTOR_HEAP_DESC DepthStencilHeapDesc = {
	  .NumDescriptors = 1, .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
	HRESULT hr =
		ID3D12Device_CreateDescriptorHeap(Renderer->Device, &DepthStencilHeapDesc, &IID_ID3D12DescriptorHeap, &Renderer->DepthStencilHeap);
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

void
SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers)
{
	D3D12_CPU_DESCRIPTOR_HANDLE RtvDescriptorHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->RtvDescriptorHeap, &RtvDescriptorHandle);
	for (UINT i = 0; i < NumBuffers; ++i) {
		HRESULT hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, i, &IID_ID3D12Resource, &Renderer->RtvBuffers[i]);
		ExitIfFailed(hr);
		ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[i], NULL, RtvDescriptorHandle);
		Renderer->RtvHandles[i] = RtvDescriptorHandle;
		RtvDescriptorHandle.ptr += Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
	}
}

void
CreateSceneResources(R_Core *const Renderer)
{
	HRESULT hr;
	D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(MEGABYTES(256), D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES UploadHeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_HEAP_PROPERTIES DefaultHeapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->UploadBuffer);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &Renderer->VertexBufferDefault);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &Renderer->IndexBufferDefault);
	ExitIfFailed(hr);

	BufferDesc.Width = MEGABYTES(8);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->MeshDataUploadBuffer);
	ExitIfFailed(hr);

	BufferDesc.Width = MEGABYTES(1);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Renderer->SceneDataUploadBuffer);
	ExitIfFailed(hr);

	D3D12_HEAP_PROPERTIES HeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	Renderer->TextureUploadBuffer.Size = MEGABYTES(256);
	Renderer->TextureUploadBuffer.CurrentOffset = 0;
	D3D12_RESOURCE_DESC TextureBufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(Renderer->TextureUploadBuffer.Size, D3D12_RESOURCE_FLAG_NONE, 0);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &TextureBufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
											  &Renderer->TextureUploadBuffer.Buffer);
	ExitIfFailed(hr);

	/* Not sure if ideal, but I decided to maintain my buffers mapped during the whole lifetime of the engine */
	D3D12_RANGE Range = {0, 0};
	ID3D12Resource_Map(Renderer->TextureUploadBuffer.Buffer, 0, &Range, &Renderer->TextureUploadBuffer.BaseMappedPtr);
	ID3D12Resource_Map(Renderer->MeshDataUploadBuffer, 0, &Range, &Renderer->MeshDataUploadBufferCpuAddress);
	ID3D12Resource_Map(Renderer->SceneDataUploadBuffer, 0, &Range, &Renderer->SceneDataUploadBufferCpuAddress);
	ID3D12Resource_Map(Renderer->UploadBuffer, 0, NULL, &Renderer->UploadBufferCpuAddress);

	CreateBaseEngineTextures(Renderer);
	R_CreateGrid(Renderer, 100.f);
}

void
CreateBaseEngineTextures(R_Core *const Renderer)
{
	WCHAR LampImagePath[MAX_PATH];
	Win32FullPath(L"/assets/images/lamp.png", LampImagePath, _countof(LampImagePath));
	R_CreateCustomTexture(LampImagePath, Renderer);
	memcpy(Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset, BillboardVertices, sizeof(BillboardVertices));
	Renderer->BillboardBufferLocation = M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset);
	Renderer->SceneDataOffset += CB_ALIGN(BillboardVertices);
}

void
CreateShaders(R_Core *const Renderer)
{
	R_CreatePBRPipelineState(Renderer);
	R_CreateBillboardPipelineState(Renderer);
	R_CreateGridPipelineState(Renderer);
}

R_SceneData
PreprocessSceneData(const S_Scene *const Scene)
{
	R_SceneData Result = {0};
	Result.CameraPosition = Scene->Data.CameraPosition;
	R_UpdateLights(Scene->ActiveLightMask, Scene->Data.Lights, Result.Lights, PBR_MAX_LIGHT_NUMBER);
	return Result;
}

void
Draw(R_Core *const Renderer, const R_Camera *const Camera, const S_Scene *const Scene)
{
	UINT64 StartMeshDataOffset = Renderer->MeshDataOffset;
	UINT64 StartSceneDataOffset = Renderer->SceneDataOffset;
	R_MeshConstants MeshConstants = {.MVP = {
									   .View = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection),
									   .Proj = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f),
									 }};
	RenderPrimitives(Scene, Renderer, &MeshConstants);
	if (Renderer->bDrawGrid) {
		R_RenderGrid(Renderer, &MeshConstants);
	}
	R_RenderLightBillboards(Renderer, Scene->Data.Lights, Scene->ActiveLightMask, &MeshConstants);
	UI_Draw(Renderer->CommandList);

	Renderer->MeshDataOffset = StartMeshDataOffset;
	Renderer->SceneDataOffset = StartSceneDataOffset;
}

void
GetAdapter(IDXGIFactory2 *Factory, R_Core *Renderer)
{
	for (UINT i = 0; SUCCEEDED(IDXGIFactory2_EnumAdapters1(Factory, i, &Renderer->Adapter)); ++i) {
		DXGI_ADAPTER_DESC1 Desc = {0};
		HRESULT hr = IDXGIAdapter3_GetDesc1(Renderer->Adapter, &Desc);
		if (FAILED(hr) || Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			IDXGIAdapter3_Release(Renderer->Adapter);
			continue;
		}
		break;
	}
}