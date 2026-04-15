#pragma once

#include "assets/gltf.h"
#include "core/scene.h"
#include "render_types.h"

#define FRAME_COUNT 2

typedef struct R_Camera R_Camera;
typedef struct R_Primitive R_Primitive;
typedef struct R_Texture R_Texture;
typedef struct TextureLookup TextureLookup;

typedef enum EReservedSrvIndex {
	ERSI_BILLBOARD_LAMP = 0,
} EReservedSrvIndex;

typedef enum ERenderState { ERS_GLTF, ERS_WIREFRAME, ERS_BILLBOARD, ERS_GRID, ERS_N_RENDER_STATES } ERenderState;

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

	IDXGIAdapter3 *Adapter;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	ID3D12Device *Device;
	IDXGISwapChain1 *SwapChain;
	ID3D12DescriptorHeap *RtvDescriptorHeap;

	ID3D12Resource *RtvBuffers[FRAME_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[FRAME_COUNT];
	UINT RtvIndex;

	ID3D12DescriptorHeap *TexturesHeap;
	UINT TexturesCount;
	R_UploadBuffer TextureUploadBuffer;
	TextureLookup *Textures;

	ID3D12RootSignature *RootSignPBR;
	ID3D12RootSignature *RootSignBillboard;
	ID3D12RootSignature *RootSignGrid;

	ID3D12Resource *DepthStencil;
	ID3D12DescriptorHeap *DepthStencilHeap;

	ID3D12CommandQueue *CommandQueue;
	ID3D12CommandAllocator *CommandAllocator;
	ID3D12GraphicsCommandList *CommandList;
	ID3D12CommandAllocator *UploadCommandAllocator;
	ID3D12GraphicsCommandList *UploadCommandList;

	ERenderState State;
	BOOL bDrawGrid;
	ID3D12PipelineState *PipelineState[ERS_N_RENDER_STATES];

	UINT DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

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
	ID3D12Resource *UploadBuffer;
	UINT8 *UploadBufferCpuAddress;

	UINT8 *MeshDataUploadBufferCpuAddress;
	ID3D12Resource *MeshDataUploadBuffer;
	UINT64 MeshDataOffset;

	UINT8 *SceneDataUploadBufferCpuAddress;
	ID3D12Resource *SceneDataUploadBuffer;
	UINT64 SceneDataOffset;

	D3D12_GPU_VIRTUAL_ADDRESS BillboardBufferLocation;
	D3D12_GPU_VIRTUAL_ADDRESS GridBufferLocation;

	UINT64 CurrentUploadBufferOffset;
	UINT64 CurrentVertexBufferOffset;
	UINT64 CurrentIndexBufferOffset;
} R_Core;

void R_Init(R_Core *const Renderer, HWND hWnd);
void R_Destroy(R_Core *Renderer);
void R_Draw(R_Core *const Renderer, const S_Scene *const Scene, const R_Camera *const Camera);
void Draw(R_Core *const Renderer, const R_Camera *const Camera, const S_Scene *const Scene);
void R_ExecuteCommands(R_Core *const Renderer, ID3D12GraphicsCommandList *CommandList, ID3D12CommandAllocator *CommandAllocator);
void R_SwapchainResize(R_Core *const Renderer, INT Width, INT Height);
