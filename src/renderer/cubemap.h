#pragma once

#include "DirectXMathC.h"

typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12Resource ID3D12Resource;
typedef struct ID3D12DescriptorHeap ID3D12DescriptorHeap;
typedef struct ID3D12GraphicsCommandList ID3D12GraphicsCommandList;
typedef struct ID3D12RootSignature ID3D12RootSignature;
typedef struct ID3D12PipelineState ID3D12PipelineState;
typedef struct R_Core R_Core;

typedef struct R_Cubemap {
	ID3D12Resource *Resource;
	ID3D12DescriptorHeap *DescriptorHeap;
	ID3D12Resource *ConstantBufferUploadHeap;
	UINT8 *MappedCBVData;

	ID3D12Resource *VertexBuffer;
	ID3D12Resource *IndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW CubeVBView;
	D3D12_INDEX_BUFFER_VIEW CubeIBView;

	UINT RTVDescriptorSize;
	D3D12_GPU_DESCRIPTOR_HANDLE GpuSrvHandle;

	UINT Width;
	UINT Height;
} R_Cubemap;

VOID R_SetupCubemapResources(R_Core *Renderer, R_Cubemap *const Cubemap, UINT Width, UINT Height);
VOID R_DrawToCubemapFaces(
	R_Core *Renderer, R_Cubemap *Target, ID3D12PipelineState *PSO, ID3D12RootSignature *RootSign, D3D12_GPU_DESCRIPTOR_HANDLE SourceSRV);
VOID R_DrawSkybox(R_Core *Renderer, XMMATRIX View, XMMATRIX Proj);