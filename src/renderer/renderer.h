#pragma once

#include "../assets/gltf.h"
#include "../core/scene.h"
#include "render_types.h"

#define FRAME_COUNT 2

typedef struct R_Camera R_Camera;
typedef struct R_Primitive R_Primitive;
typedef struct R_Texture R_Texture;

typedef enum ERenderState { ERS_GLTF, ERS_WIREFRAME, ERS_N_RENDER_STATES } ERenderState;

typedef struct GPUTexture {
	ID3D12Resource *GpuTexture;
	D3D12_GPU_DESCRIPTOR_HANDLE SrvHandle;
} GPUTexture;

typedef struct TextureLookup {
	char *key;
	GPUTexture Texture;
} TextureLookup;

typedef struct R_UploadBuffer {
	ID3D12Resource *Buffer;
	UINT8 *BaseMappedPtr;
	UINT64 Size;
	UINT64 CurrentOffset;
} R_UploadBuffer;

typedef struct R_Core {
	HWND hWnd;
	UINT Width;
	UINT Height;
	FLOAT AspectRatio;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	ID3D12Device *Device;
	IDXGISwapChain1 *SwapChain;
	ID3D12DescriptorHeap *RtvDescriptorHeap;

	ID3D12Resource *RtvBuffers[FRAME_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[FRAME_COUNT];
	UINT RtvDescIncrement;
	UINT RtvIndex;
	
	ID3D12DescriptorHeap *SrvHeap;
	UINT SrvCount;


	ID3DBlob *VS;
	ID3DBlob *PS;
	ID3D12RootSignature *RootSign;

	ID3D12Resource *DepthStencil;
	ID3D12DescriptorHeap *DepthStencilHeap;

	ID3D12CommandQueue *CommandQueue;
	ID3D12CommandAllocator *CommandAllocator;
	ID3D12GraphicsCommandList *CommandList;

	ERenderState State;
	ID3D12PipelineState *PipelineState[ERS_N_RENDER_STATES];

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

	ID3D12Resource *VertexBufferDefault;
	ID3D12Resource *IndexBufferDefault;
	ID3D12Resource *VertexBufferUpload;

	UINT8 *MeshDataUploadBufferCpuAddress;
	ID3D12Resource *MeshDataUploadBuffer;

	UINT8 *SceneDataUploadBufferCpuAddress;
	ID3D12Resource *SceneDataUploadBuffer;

	R_UploadBuffer TextureUploadBuffer;

	ID3D12Resource *MaterialBuffer;
	TextureLookup *Textures;
} R_Core;

void R_Init(R_Core *const Renderer, HWND hWnd);
void R_Destroy(R_Core *Renderer);
void R_Draw(R_Core *const Renderer, S_Scene *Scene, R_Camera *const Camera);
void R_UpdateResource(ID3D12Resource *Resource, void *Data, size_t DataSize);
void R_ExecuteCommands(R_Core *const Renderer);
void R_SwapchainResize(R_Core *const Renderer, INT Width, INT Height);
void R_CreateUITexture(PCWSTR Path, R_Core *Renderer, UINT nkSlotIndex);
GPUTexture R_UploadTexture(R_Core *Renderer, R_Texture *Source, UINT SlotIndex);
