#pragma once

#include "../assets/gltf.h"
#include "../core/scene.h"

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

typedef struct R_World {
	HWND hWnd;
	UINT Width;
	UINT Height;
	FLOAT AspectRatio;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;
	IDXGISwapChain1 *SwapChain;
	ID3D12DescriptorHeap *RtvDescriptorHeap;

	ID3D12Resource *RtvBuffers[FRAME_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[FRAME_COUNT];
	UINT RtvDescIncrement;
	UINT RtvIndex;
	
	ID3D12DescriptorHeap *SrvHeap;
	UINT SrvCount;

	ID3D12Device *Device;
	ID3D12CommandQueue *CommandQueue;

	ID3DBlob *VS;
	ID3DBlob *PS;
	ID3D12RootSignature *RootSign;

	ID3D12Resource *DepthStencil;
	ID3D12DescriptorHeap *DepthStencilHeap;

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

	UINT8 *LightDataUploadBufferCpuAddress;
	ID3D12Resource *LightDataUploadBuffer;

	R_UploadBuffer TextureUploadBuffer;

	ID3D12Resource *MaterialBuffer;
	TextureLookup *Textures;
} R_World;

void R_Init(R_World *const Renderer, HWND hWnd);
void R_Destroy(R_World *Renderer);
void R_Draw(R_World *const Renderer, S_Scene *Scene, R_Camera *const Camera);
void R_UpdateResource(ID3D12Resource *Resource, void *Data, size_t DataSize);
void R_ExecuteCommands(R_World *const Renderer);
void R_SwapchainResize(R_World *const Renderer, INT Width, INT Height);
void R_CreateUITexture(PCWSTR Path, R_World *Renderer, UINT nkSlotIndex);
GPUTexture R_UploadTexture(R_World *Renderer, R_Texture *Source, UINT SlotIndex);
