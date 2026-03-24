#include "pch.h"

#include "grid.h"
#include "../renderer/render_types.h"
#include "../renderer/renderer.h"

#include <DirectXMathC.h>

#define GRID_VERTICES_COUNT 300

void
R_RenderGrid(R_MeshConstants *const MeshConstants, R_Core *const Renderer, const float HalfSide)
{
	MeshConstants->Model = XMMatrixIdentity();
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignGrid);
	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[ERS_GRID]);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	M_UpdateResourceData(Renderer->MeshDataUploadBuffer, MeshConstants, sizeof(R_MeshConstants), Renderer->MeshDataOffset);

	D3D12_GPU_VIRTUAL_ADDRESS MeshDataGpuAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->MeshDataUploadBuffer);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, MeshDataGpuAddress + Renderer->MeshDataOffset);

	XMFLOAT3 Vertices[GRID_VERTICES_COUNT] = {0};
	const INT LinesPerDirection = (GRID_VERTICES_COUNT / 4);
	const FLOAT Step = (HalfSide * 2.0f) / (float)(LinesPerDirection - 1);

	for (INT Line = 0; Line < LinesPerDirection; Line++) {
		FLOAT Position = -HalfSide + (Line * Step);
		INT i = Line * 4;
		Vertices[i] = (XMFLOAT3){Position, 0.0f, -HalfSide};
		Vertices[i + 1] = (XMFLOAT3){Position, 0.0f, HalfSide};
		Vertices[i + 2] = (XMFLOAT3){-HalfSide, 0.0f, Position};
		Vertices[i + 3] = (XMFLOAT3){HalfSide, 0.0f, Position};
	}

	size_t VerticesByteSize = GRID_VERTICES_COUNT * sizeof(XMFLOAT3);
	
	M_UpdateResourceData(Renderer->SceneDataUploadBuffer, &Vertices, sizeof(Vertices), Renderer->SceneDataOffset);

	D3D12_VERTEX_BUFFER_VIEW VBV = {
	  .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->SceneDataUploadBuffer) + Renderer->SceneDataOffset,
	  .SizeInBytes = sizeof(Vertices),
	  .StrideInBytes = sizeof(XMFLOAT3),
	};

	ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &VBV);
	ID3D12GraphicsCommandList_DrawInstanced(Renderer->CommandList, GRID_VERTICES_COUNT, 1, 0, 0);

	Renderer->SceneDataOffset += CB_ALIGN(Vertices);
	Renderer->MeshDataOffset += CB_ALIGN(R_MeshConstants);
}