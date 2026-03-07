#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "../assets/gltf.h"
#include "../core/scene.h"

#define FRAME_COUNT 2

typedef struct R_Camera R_Camera;

typedef struct R_World {
	HWND hWnd;
	UINT Width;
	UINT Height;
	float AspectRatio;
	WCHAR AssetsPath[512];

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	DXGI_MODE_DESC FullscreenMode;
	BOOL bFullscreen;
	DXGI_MODE_DESC *DisplayModes;

	IDXGISwapChain1 *SwapChain;
	ID3D12DescriptorHeap *RtvDescriptorHeap;
	ID3D12DescriptorHeap *SrvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[FRAME_COUNT];
	ID3D12Resource *RtvBuffers[FRAME_COUNT];
	UINT RtvDescIncrement;
	UINT RtvIndex;

	ID3D12Device *Device;
	ID3D12CommandQueue *CommandQueue;

	ID3D12CommandAllocator *CommandAllocator;
	ID3D12GraphicsCommandList *CommandList;

	ID3D12PipelineState *PipelineStateScene;

	/*****************************
		Synchronization objects
	*****************************/

	UINT FrameIndex;
	UINT64 FenceValue;
	HANDLE FenceEvent;
	ID3D12Fence *Fence;

	/*****************************
		Resources
	*****************************/

	ID3D12Resource *ConstantBuffer;
	ID3D12Resource *ModelGpuTexture;

	UINT SrvDescriptorSize;
	D3D12_GPU_DESCRIPTOR_HANDLE ModelGpuSrv;
} R_World;

void R_Init(R_World *const Renderer, HWND hWnd);
void R_CreateVertexBuffer(ID3D12Device *Device, R_Mesh *const mesh);
void R_CreateIndexBuffer(ID3D12Device *Device, R_Mesh *const mesh);
void R_UploadTexture(R_World *Renderer, R_Texture *Source, ID3D12Resource **OutTexture, D3D12_GPU_DESCRIPTOR_HANDLE *OutSrv, UINT SrvIndex);
void R_Destroy(R_World *Renderer);
void R_Update(R_World *const Renderer, R_Camera *const Camera, SendaiScene *Scene);
void R_Draw(R_World *const Renderer, SendaiScene *Scene);
void R_ExecuteCommands(R_World *const Renderer);
void R_SwapchainResize(R_World *const Renderer, int Width, int Height);