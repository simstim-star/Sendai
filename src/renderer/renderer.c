#include "core/pch.h"

#include "light.h"
#include "render_types.h"
#include "renderer.h"
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

#include "billboard.h"
#include "stb_ds.h"

typedef enum
{
	ERPT_PASS_OPAQUE,
	ERPT_PASS_BLEND
} ERenderPassType;

static const FLOAT CUBEMAP_CLEAR_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static VOID SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers);
static VOID GetAdapter(IDXGIFactory2 *Factory, R_Core *Renderer);

static VOID SignalAndWait(R_Core *const Renderer);
static VOID WaitForFenceValue(R_Core *const Renderer, UINT64 Value);

static VOID CreateSceneResources(R_Core *const Renderer);
static VOID CreateBaseEngineTextures(R_Core *const Renderer);
static VOID CreateDepthStencilBuffer(R_Core *const Renderer);
static VOID CreateShaders(R_Core *const Renderer);

static R_SceneData PreprocessSceneData(const S_Scene *const Scene);

static VOID AdvanceFrame(R_Core *const Renderer);

static VOID CmdList_RegisterSetupCommands(R_Core *const Renderer);
static VOID CmdList_RegisterDrawCommands(R_Core *const Renderer, const R_Camera *const Camera, const S_Scene *const Scene);
static VOID CmdList_RegisterDrawPBR(const S_Scene *const Scene, R_Core *const Renderer, R_MeshConstants *const MeshConstants);
static VOID CmdList_RegisterRenderPassPBR(const S_Scene *Scene,
										  R_Core *const Renderer,
										  R_MeshConstants *const MeshConstants,
										  const ERenderPassType Pass);
static VOID CmdList_Submit(const R_Core *const Renderer);

/****************************************************
Public functions
*****************************************************/

VOID
R_Init(R_Core *const Renderer, HWND hWnd)
{
	Renderer->hWnd = hWnd;
	Renderer->AspectRatio = (FLOAT)(Renderer->Width) / (Renderer->Height);
	Renderer->Viewport = (D3D12_VIEWPORT){
	  .TopLeftX = 0.0f,
	  .TopLeftY = 0.0f,
	  .Width = (FLOAT)(Renderer->Width),
	  .Height = (FLOAT)(Renderer->Height),
	  .MinDepth = 0.0f,
	  .MaxDepth = 1.0f,
	};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(Renderer->Width), (LONG)(Renderer->Height)};
	Renderer->State = ERS_GLTF;
	Renderer->bDrawGrid = TRUE;

	/* D3D12 setup */

	INT bIsDebugFactory = 0;
#if defined(_DEBUG)
	ID3D12Debug1 *DebugController = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (VOID **)&DebugController)))
	{
		ID3D12Debug1_EnableDebugLayer(DebugController);
		// ID3D12Debug1_SetEnableGPUBasedValidation(debug_controller, 1);
		bIsDebugFactory |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1_Release(DebugController);
	}
#endif

	IDXGIFactory2 *Factory = NULL;
	HRESULT hr = CreateDXGIFactory2(bIsDebugFactory, &IID_IDXGIFactory2, &Factory);
	ExitIfFailed(hr);
	_Analysis_assume_(Factory != NULL);

	GetAdapter(Factory, Renderer);

	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &Renderer->Device);
	ExitIfFailed(hr);
	_Analysis_assume_(Renderer->Device != NULL);

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {
	  .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	  .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	  .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateCommandQueue(Renderer->Device, &CommandQueueDesc, &IID_ID3D12CommandQueue, &Renderer->CommandQueue);
	ExitIfFailed(hr);

	Renderer->FenceValue = 0;
	hr = ID3D12Device_CreateFence(Renderer->Device, Renderer->FenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence,
								  &Renderer->Fence);
	ExitIfFailed(hr);

	for (UINT Frame = 0; Frame < FRAME_COUNT; ++Frame)
	{
		hr = ID3D12Device_CreateCommandAllocator(Renderer->Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
												 &Renderer->CommandAllocators[Frame]);
		ExitIfFailed(hr);
	}

	Renderer->FrameIndex = 0;
	hr = ID3D12Device_CreateCommandList(
		Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->CommandAllocators[Renderer->FrameIndex],
		Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1, &Renderer->CommandList);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandAllocator(Renderer->Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
											 &Renderer->UploadCommandAllocator);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommandList(Renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, Renderer->UploadCommandAllocator,
										Renderer->PipelineState[Renderer->State], &IID_ID3D12GraphicsCommandList1,
										&Renderer->UploadCommandList);
	ExitIfFailed(hr);

	Renderer->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (Renderer->FenceEvent == NULL)
	{
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
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &RtvDescHeapDesc, &IID_ID3D12DescriptorHeap,
										   &Renderer->RtvDescriptorHeap);
	ExitIfFailed(hr);

	for (D3D12_DESCRIPTOR_HEAP_TYPE Type = 0; Type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++Type)
	{
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
	  .BufferCount = FRAME_COUNT,
	  .Scaling = DXGI_SCALING_STRETCH,
	  .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
	  .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
	  .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
	};
	IDXGISwapChain1 *SwapChain1 = NULL;
	hr = IDXGIFactory2_CreateSwapChainForHwnd(Factory, (IUnknown *)Renderer->CommandQueue, Renderer->hWnd, &SwapChainDesc, NULL,
											  NULL, &SwapChain1);
	ExitIfFailed(hr);

	hr = IDXGISwapChain1_QueryInterface(SwapChain1, &IID_IDXGISwapChain3, &Renderer->SwapChain);
	ExitIfFailed(hr);
	IDXGISwapChain1_Release(SwapChain1);

	SetRtvBuffers(Renderer, FRAME_COUNT);
	CreateSceneResources(Renderer);
	CreateDepthStencilBuffer(Renderer);
	CreateShaders(Renderer);

	IDXGIFactory2_Release(Factory);
}

VOID
R_Draw(R_Core *const Renderer, const S_Scene *const Scene, const R_Camera *const Camera)
{
	CmdList_RegisterSetupCommands(Renderer);
	CmdList_RegisterDrawCommands(Renderer, Camera, Scene);
	CmdList_Submit(Renderer);

	HRESULT hr = IDXGISwapChain3_Present(Renderer->SwapChain, 1, 0);
	ExitIfFailed(hr);

	AdvanceFrame(Renderer);
}

VOID
CmdList_RegisterSetupCommands(R_Core *const Renderer)
{
	Renderer->RtvIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(Renderer->SwapChain);

	ID3D12GraphicsCommandList_RSSetViewports(Renderer->CommandList, 1, &Renderer->Viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(Renderer->CommandList, 1, &Renderer->ScissorRect);

	D3D12_RESOURCE_BARRIER TransitionRenderTargetToRenderState = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition.pResource = Renderer->RtvBuffers[Renderer->RtvIndex],
	  .Transition.Subresource = 0,
	  .Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
	  .Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
	  .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
	};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &TransitionRenderTargetToRenderState);

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCPUHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->DepthStencilHeap, &DepthStencilCPUHandle);
	ID3D12GraphicsCommandList_OMSetRenderTargets(Renderer->CommandList, 1, &Renderer->RtvHandles[Renderer->RtvIndex], FALSE,
												 &DepthStencilCPUHandle);

	ID3D12GraphicsCommandList_ClearRenderTargetView(Renderer->CommandList, Renderer->RtvHandles[Renderer->RtvIndex],
													CUBEMAP_CLEAR_COLOR, 0, NULL);
	ID3D12GraphicsCommandList_ClearDepthStencilView(Renderer->CommandList, DepthStencilCPUHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0,
													0, NULL);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap *Heaps[] = {Renderer->TexturesHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, 1, Heaps);

	Renderer->MeshDataOffset = Renderer->FrameIndex * MESH_DATA_FRAME_SIZE;
	Renderer->SceneDataOffset = SCENE_DATA_STATIC_SIZE + Renderer->FrameIndex * SCENE_DATA_FRAME_SIZE;
}

VOID
CmdList_Submit(const R_Core *const Renderer)
{
	D3D12_RESOURCE_BARRIER TransitionRenderTargetToPresentState = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition.pResource = Renderer->RtvBuffers[Renderer->RtvIndex],
	  .Transition.Subresource = 0,
	  .Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
	  .Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT,
	  .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
	};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &TransitionRenderTargetToPresentState);

	HRESULT hr = ID3D12GraphicsCommandList_Close(Renderer->CommandList);
	ExitIfFailed(hr);

	ID3D12CommandList *CmdLists[] = {(ID3D12CommandList *)Renderer->CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(Renderer->CommandQueue, 1, CmdLists);
}

VOID
CmdList_SubmitAndWait(R_Core *const Renderer, ID3D12GraphicsCommandList *CommandList, ID3D12CommandAllocator *CommandAllocator)
{
	HRESULT hr = ID3D12GraphicsCommandList_Close(CommandList);
	ExitIfFailed(hr);
	ID3D12CommandList *CmdLists[] = {(ID3D12CommandList *)CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(Renderer->CommandQueue, 1, CmdLists);
	SignalAndWait(Renderer);
	ID3D12CommandAllocator_Reset(CommandAllocator);
	ID3D12GraphicsCommandList_Reset(CommandList, CommandAllocator, Renderer->PipelineState[Renderer->State]);
}

VOID
R_SwapchainResize(R_Core *const Renderer, INT Width, INT Height)
{
	SignalAndWait(Renderer);

	for (UINT i = 0; i < FRAME_COUNT; ++i)
	{
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}

	HRESULT hr = IDXGISwapChain3_ResizeBuffers(Renderer->SwapChain, FRAME_COUNT, Width, Height, DXGI_FORMAT_UNKNOWN,
											   DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	ExitIfFailed(hr);
	SetRtvBuffers(Renderer, FRAME_COUNT);

	Renderer->Width = Width;
	Renderer->Height = Height;
	Renderer->AspectRatio = (FLOAT)Width / (FLOAT)Height;
	Renderer->Viewport = (D3D12_VIEWPORT){
	  .TopLeftX = 0.0f,
	  .TopLeftY = 0.0f,
	  .Width = (FLOAT)Width,
	  .Height = (FLOAT)Height,
	  .MinDepth = 0.0f,
	  .MaxDepth = 1.0f,
	};
	Renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)Width, (LONG)Height};

	if (Renderer->DepthStencil)
	{
		ID3D12Resource_Release(Renderer->DepthStencil);
		ID3D12DescriptorHeap_Release(Renderer->DepthStencilHeap);
	}

	CreateDepthStencilBuffer(Renderer);
}

VOID
R_Destroy(R_Core *Renderer)
{
	SignalAndWait(Renderer);

	UI_Destroy();

	for (ERenderState State = ERS_GLTF; State < ERS_N_RENDER_STATES; ++State)
	{
		ID3D12PipelineState_Release(Renderer->PipelineState[State]);
	}

	ID3D12Resource_Release(Renderer->DepthStencil);
	ID3D12Resource_Release(Renderer->VertexBufferDefault);
	ID3D12Resource_Release(Renderer->IndexBufferDefault);
	ID3D12Resource_Release(Renderer->UploadBuffer);
	ID3D12Resource_Release(Renderer->SceneDataUploadBuffer);
	IDXGIAdapter3_Release(Renderer->Adapter);

	for (INT i = 0; i < hmlen(Renderer->Textures); ++i)
	{
		ID3D12Resource_Release(Renderer->Textures[i].Texture.GpuTexture);
	}

	for (INT i = 0; i < FRAME_COUNT; ++i)
	{
		ID3D12Resource_Release(Renderer->RtvBuffers[i]);
	}
	CloseHandle(Renderer->FenceEvent);

	R_CubemapRelease(&Renderer->Cubemap);
	R_CubemapRelease(&Renderer->IrradianceMap);

	ID3D12DescriptorHeap_Release(Renderer->RtvDescriptorHeap);
	ID3D12DescriptorHeap_Release(Renderer->TexturesHeap);
	ID3D12DescriptorHeap_Release(Renderer->DepthStencilHeap);

	ID3D12Resource_Release(Renderer->MeshDataUploadBuffer);
	ID3D12Resource_Release(Renderer->TextureUploadBuffer.Buffer);

	ID3D12RootSignature_Release(Renderer->RootSignPBR);
	ID3D12RootSignature_Release(Renderer->RootSignBillboard);
	ID3D12RootSignature_Release(Renderer->RootSignCubemap);
	ID3D12RootSignature_Release(Renderer->RootSignIrradiance);
	ID3D12RootSignature_Release(Renderer->RootSignSkybox);
	ID3D12RootSignature_Release(Renderer->RootSignGrid);

	for (UINT Frame = 0; Frame < FRAME_COUNT; ++Frame)
	{
		ID3D12CommandAllocator_Release(Renderer->CommandAllocators[Frame]);
	}
	ID3D12GraphicsCommandList_Release(Renderer->CommandList);
	ID3D12CommandAllocator_Release(Renderer->UploadCommandAllocator);
	ID3D12GraphicsCommandList_Release(Renderer->UploadCommandList);
	ID3D12CommandQueue_Release(Renderer->CommandQueue);
	ID3D12Fence_Release(Renderer->Fence);
	IDXGISwapChain3_Release(Renderer->SwapChain);
	ID3D12Device_Release(Renderer->Device);

	shfree(Renderer->Textures);
}

/****************************************************
	Implementation of private functions
*****************************************************/

VOID
SignalAndWait(R_Core *const Renderer)
{
	HRESULT hr = ID3D12CommandQueue_Signal(Renderer->CommandQueue, Renderer->Fence, ++Renderer->FenceValue);
	ExitIfFailed(hr);
	WaitForFenceValue(Renderer, Renderer->FenceValue);
}

VOID
WaitForFenceValue(R_Core *const Renderer, UINT64 Value)
{
	if (ID3D12Fence_GetCompletedValue(Renderer->Fence) < Value)
	{
		HRESULT hr = ID3D12Fence_SetEventOnCompletion(Renderer->Fence, Value, Renderer->FenceEvent);
		ExitIfFailed(hr);
		WaitForSingleObject(Renderer->FenceEvent, INFINITE);
	}
}

VOID
AdvanceFrame(R_Core *const Renderer)
{
	const UINT64 SignalValue = ++Renderer->FenceValue;
	HRESULT hr = ID3D12CommandQueue_Signal(Renderer->CommandQueue, Renderer->Fence, SignalValue);
	ExitIfFailed(hr);

	Renderer->FrameFenceValues[Renderer->FrameIndex] = SignalValue;
	Renderer->FrameIndex = (Renderer->FrameIndex + 1) % FRAME_COUNT;
	WaitForFenceValue(Renderer, Renderer->FrameFenceValues[Renderer->FrameIndex]);

	ID3D12CommandAllocator *NextFrameAllocator = Renderer->CommandAllocators[Renderer->FrameIndex];
	hr = ID3D12CommandAllocator_Reset(NextFrameAllocator);
	ExitIfFailed(hr);
	hr = ID3D12GraphicsCommandList_Reset(Renderer->CommandList, NextFrameAllocator, Renderer->PipelineState[Renderer->State]);
	ExitIfFailed(hr);
}

VOID
CmdList_RegisterRenderPassPBR(const S_Scene *Scene,
							  R_Core *const Renderer,
							  R_MeshConstants *const MeshConstants,
							  const ERenderPassType Pass)
{
	ERenderState PSO;
	switch (Renderer->State)
	{
	case ERS_WIREFRAME:
		PSO = ERS_WIREFRAME;
		break;
	default:
		PSO = (Pass == ERPT_PASS_OPAQUE) ? ERS_GLTF : ERS_GLTF_BLEND;
		break;
	}

	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[PSO]);
	for (size_t ModelIndex = 0; ModelIndex < Scene->ModelsCount; ++ModelIndex)
	{
		const R_Model *const Model = &Scene->Models[ModelIndex];
		if (!Model->Visible)
		{
			continue;
		}

		const XMMATRIX T = XMMatrixTranslation(Model->Position.x, Model->Position.y, Model->Position.z);
		const XMMATRIX R = XMMatrixRotationRollPitchYaw(Model->Rotation.x, Model->Rotation.y, Model->Rotation.z);
		const XMMATRIX S = XMMatrixScaling(Model->Scale.x, Model->Scale.y, Model->Scale.z);
		XMMATRIX M = XM_MAT_MULT(S, R);
		M = XM_MAT_MULT(M, T);

		for (size_t NodeIndex = 0; NodeIndex < Model->NodesCount; ++NodeIndex)
		{
			R_Node *Node = &Model->Nodes[NodeIndex];

			if (Pass == ERPT_PASS_OPAQUE)
			{
				MeshConstants->MVP.Model = XMLoadFloat4x4(&Node->ModelMatrix);
				MeshConstants->MVP.Model = XM_MAT_MULT(MeshConstants->MVP.Model, M);

				XMFLOAT4X4 ModelFloat;
				XM_STORE_FLOAT4X4(&ModelFloat, MeshConstants->MVP.Model);
				MeshConstants->Normal = R_NormalMatrix(&ModelFloat);

				memcpy(Renderer->MeshDataUploadBufferCpuAddress + Renderer->MeshDataOffset, MeshConstants,
					   sizeof(R_MeshConstants));
			}

			ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
				Renderer->CommandList, 0, M_GpuAddress(Renderer->MeshDataUploadBuffer, Renderer->MeshDataOffset));
			Renderer->MeshDataOffset += CB_ALIGN(R_MeshConstants);

			if (Node->Mesh)
			{
				UINT64 PrimitiveCount =
					(Pass == ERPT_PASS_OPAQUE) ? Node->Mesh->OpaquePrimitivesCount : Node->Mesh->BlendedPrimitivesCount;
				R_Primitive *Primitives =
					(Pass == ERPT_PASS_OPAQUE) ? Node->Mesh->OpaquePrimitives : Node->Mesh->BlendedPrimitives;
				for (INT PrimitiveIndex = 0; PrimitiveIndex < PrimitiveCount; ++PrimitiveIndex)
				{
					R_Primitive *Primitive = &Primitives[PrimitiveIndex];
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

VOID
CmdList_RegisterDrawPBR(const S_Scene *const Scene, R_Core *const Renderer, R_MeshConstants *const MeshConstants)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignPBR);

	R_SceneData SceneData = PreprocessSceneData(Scene);
	memcpy(Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset, &SceneData, sizeof(R_SceneData));
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(
		Renderer->CommandList, 2, M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset));
	Renderer->SceneDataOffset += CB_ALIGN(R_SceneData);

	D3D12_GPU_DESCRIPTOR_HANDLE TexturesHeapStart;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &TexturesHeapStart);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 3, TexturesHeapStart);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 4, Renderer->IrradianceMap.GpuSrvHandle);

	UINT64 PassStartOffset = Renderer->MeshDataOffset;
	CmdList_RegisterRenderPassPBR(Scene, Renderer, MeshConstants, ERPT_PASS_OPAQUE);
	Renderer->MeshDataOffset = PassStartOffset;
	CmdList_RegisterRenderPassPBR(Scene, Renderer, MeshConstants, ERPT_PASS_BLEND);
}

VOID
CreateDepthStencilBuffer(R_Core *const Renderer)
{
	D3D12_DESCRIPTOR_HEAP_DESC DepthStencilHeapDesc = {
	  .NumDescriptors = 1, .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
	HRESULT hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &DepthStencilHeapDesc, &IID_ID3D12DescriptorHeap,
												   &Renderer->DepthStencilHeap);
	ExitIfFailed(hr);

	D3D12_RESOURCE_DESC TextureDesc = CD3DX12_TEX2D(DXGI_FORMAT_D32_FLOAT, Renderer->Width, Renderer->Height, 1, 0, 1, 0,
													D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
	D3D12_CLEAR_VALUE DepthOptimizedClearValue = {.Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil.Depth = 1.0f};
	D3D12_HEAP_PROPERTIES DefaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeap, D3D12_HEAP_FLAG_NONE, &TextureDesc,
										 D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthOptimizedClearValue, &IID_ID3D12Resource,
										 &Renderer->DepthStencil);

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->DepthStencilHeap, &DepthStencilHandle);
	ID3D12Device_CreateDepthStencilView(Renderer->Device, Renderer->DepthStencil, NULL, DepthStencilHandle);
}

VOID
SetRtvBuffers(R_Core *const Renderer, UINT NumBuffers)
{
	for (UINT i = 0; i < NumBuffers; ++i)
	{
		HRESULT hr = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, i, &IID_ID3D12Resource, &Renderer->RtvBuffers[i]);
		ExitIfFailed(hr);
		D3D12_CPU_DESCRIPTOR_HANDLE RtvDescriptorHandle = M_CpuDescriptorHandle(
			Renderer->RtvDescriptorHeap, i * Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
		ID3D12Device_CreateRenderTargetView(Renderer->Device, Renderer->RtvBuffers[i], NULL, RtvDescriptorHandle);
		Renderer->RtvHandles[i] = RtvDescriptorHandle;
	}
}

VOID
CreateSceneResources(R_Core *const Renderer)
{
	HRESULT hr;
	D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC_BUFFER(MEGABYTES(256), D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES UploadHeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_HEAP_PROPERTIES DefaultHeapProps = {.Type = D3D12_HEAP_TYPE_DEFAULT};
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
											  &Renderer->UploadBuffer);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource,
											  &Renderer->VertexBufferDefault);
	ExitIfFailed(hr);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource,
											  &Renderer->IndexBufferDefault);
	ExitIfFailed(hr);

	BufferDesc.Width = MESH_DATA_FRAME_SIZE * FRAME_COUNT;
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
											  &Renderer->MeshDataUploadBuffer);
	ExitIfFailed(hr);

	BufferDesc.Width = SCENE_DATA_STATIC_SIZE + SCENE_DATA_FRAME_SIZE * FRAME_COUNT;
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
											  &Renderer->SceneDataUploadBuffer);
	ExitIfFailed(hr);

	D3D12_HEAP_PROPERTIES HeapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	Renderer->TextureUploadBuffer.Size = MEGABYTES(256);
	Renderer->TextureUploadBuffer.CurrentOffset = 0;
	D3D12_RESOURCE_DESC TextureBufferDesc =
		CD3DX12_RESOURCE_DESC_BUFFER(Renderer->TextureUploadBuffer.Size, D3D12_RESOURCE_FLAG_NONE, 0);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &HeapProps, D3D12_HEAP_FLAG_NONE, &TextureBufferDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
											  &Renderer->TextureUploadBuffer.Buffer);
	ExitIfFailed(hr);

	/* Not sure if ideal, but I decided to maintain my buffers mapped during the whole lifetime of the engine */
	D3D12_RANGE Range = {0};
	ID3D12Resource_Map(Renderer->TextureUploadBuffer.Buffer, 0, &Range, &Renderer->TextureUploadBuffer.BaseMappedPtr);
	ID3D12Resource_Map(Renderer->MeshDataUploadBuffer, 0, &Range, &Renderer->MeshDataUploadBufferCpuAddress);
	ID3D12Resource_Map(Renderer->SceneDataUploadBuffer, 0, &Range, &Renderer->SceneDataUploadBufferCpuAddress);
	ID3D12Resource_Map(Renderer->UploadBuffer, 0, NULL, &Renderer->UploadBufferCpuAddress);

	CreateBaseEngineTextures(Renderer);
	R_CreateGrid(Renderer, 100.f);
	R_SetupCubemapResources(Renderer, &Renderer->Cubemap, 2048, 2048);
	R_SetupCubemapResources(Renderer, &Renderer->IrradianceMap, 32, 32);
}

VOID
CreateBaseEngineTextures(R_Core *const Renderer)
{
	WCHAR LampImagePath[MAX_PATH];
	Win32FullPath(L"/assets/images/lamp.png", LampImagePath, _countof(LampImagePath));
	R_CreateCustomTexture(LampImagePath, Renderer);
	memcpy(Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset, BILLBOARD_VERTICES, sizeof(BILLBOARD_VERTICES));
	Renderer->BillboardBufferLocation = M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset);
	Renderer->SceneDataOffset += CB_ALIGN(BILLBOARD_VERTICES);
}

VOID
CreateShaders(R_Core *const Renderer)
{
	R_CreatePBRPipelineState(Renderer);
	R_CreateBillboardPipelineState(Renderer);
	R_CreateGridPipelineState(Renderer);
	R_CreateCubemapPipelineState(Renderer);
	R_CreateIrradiancePipelineState(Renderer);
	R_CreateSkyboxPipelineState(Renderer);
}

R_SceneData
PreprocessSceneData(const S_Scene *const Scene)
{
	R_SceneData Result = {0};
	Result.CameraPosition = Scene->Data.CameraPosition;
	UINT ResultIndex = 0;
	for (UINT DataIndex = 0; DataIndex < PBR_MAX_LIGHT_NUMBER; DataIndex++)
	{
		if (IS_LIGHT_ACTIVE(Scene->ActiveLightMask, DataIndex))
		{
			Result.Lights[ResultIndex].LightColor = Scene->Data.Lights[DataIndex].LightColor;
			Result.Lights[ResultIndex].LightPosition = Scene->Data.Lights[DataIndex].LightPosition;
			Result.NumActiveLights++;
			ResultIndex++;
		}
	}

	return Result;
}

VOID
CmdList_RegisterDrawCommands(R_Core *const Renderer, const R_Camera *const Camera, const S_Scene *const Scene)
{
	R_MeshConstants MeshConstants = {.MVP = {
									   .View = R_CameraViewMatrix(Camera->Position, Camera->LookDirection, Camera->UpDirection),
									   .Proj = R_CameraProjectionMatrix(XM_PIDIV4, Renderer->AspectRatio, 0.1f, 1000.0f),
									 }};
	CmdList_RegisterDrawPBR(Scene, Renderer, &MeshConstants);
	CmdList_RegisterDrawSkybox(Renderer, MeshConstants.MVP.View, MeshConstants.MVP.Proj);
	if (Renderer->bDrawGrid)
	{
		CmdList_RegisterDrawGrid(Renderer, &MeshConstants);
	}
	CmdList_RegisterDrawLightBillboards(Renderer, Scene->Data.Lights, Scene->ActiveLightMask, &MeshConstants);

	/* Nuklear rewrites a single upload buffer inside UI_Draw, so the GPU must be done with the previous frame's copy first */
	WaitForFenceValue(Renderer, Renderer->FenceValue);
	UI_Draw(Renderer->CommandList);
}

VOID
GetAdapter(IDXGIFactory2 *Factory, R_Core *Renderer)
{
	IDXGIAdapter1 *Adapter1 = NULL;
	HRESULT hr;
	for (UINT i = 0; SUCCEEDED(IDXGIFactory2_EnumAdapters1(Factory, i, &Adapter1)); ++i)
	{
		DXGI_ADAPTER_DESC1 Desc = {0};
		hr = IDXGIAdapter1_GetDesc1(Adapter1, &Desc);
		if (FAILED(hr) || Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			IDXGIAdapter1_Release(Adapter1);
			continue;
		}

		hr = IDXGIAdapter1_QueryInterface(Adapter1, &IID_IDXGIAdapter3, &Renderer->Adapter);
		IDXGIAdapter1_Release(Adapter1);

		if (SUCCEEDED(hr))
		{
			break;
		}
	}
}