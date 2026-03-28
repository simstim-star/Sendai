#include "pch.h"

#include "../renderer/render_types.h"
#include "../renderer/renderer.h"
#include "grid.h"

#include <DirectXMathC.h>

#define GRID_VERTICES_COUNT 300
static XMFLOAT3 GRID_VERTICES[GRID_VERTICES_COUNT] = {0};

void
R_CreateGrid(R_Core *const Renderer, const float HalfSide)
{
	const INT LinesPerDirection = (GRID_VERTICES_COUNT / 4);
	const FLOAT Step = (HalfSide * 2.0f) / (float)(LinesPerDirection - 1);

	for (INT Line = 0; Line < LinesPerDirection; Line++) {
		FLOAT Position = -HalfSide + (Line * Step);
		INT i = Line * 4;
		GRID_VERTICES[i] = (XMFLOAT3){Position, 0.0f, -HalfSide};
		GRID_VERTICES[i + 1] = (XMFLOAT3){Position, 0.0f, HalfSide};
		GRID_VERTICES[i + 2] = (XMFLOAT3){-HalfSide, 0.0f, Position};
		GRID_VERTICES[i + 3] = (XMFLOAT3){HalfSide, 0.0f, Position};
	}

	memcpy(Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset, &GRID_VERTICES, sizeof(GRID_VERTICES));
	Renderer->GridBufferLocation = M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset);
	Renderer->SceneDataOffset += CB_ALIGN(GRID_VERTICES);
}

void
R_RenderGrid(R_Core *const Renderer, R_MeshConstants *const MeshConstants)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignGrid);
	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[ERS_GRID]);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	MeshConstants->MVP.Model = XMMatrixIdentity();
	memcpy(Renderer->MeshDataUploadBufferCpuAddress + Renderer->MeshDataOffset, MeshConstants, sizeof(R_MeshConstants));

	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, M_GpuAddress(Renderer->MeshDataUploadBuffer, Renderer->MeshDataOffset));

	D3D12_VERTEX_BUFFER_VIEW VBV = {
	  .BufferLocation = Renderer->GridBufferLocation,
	  .SizeInBytes = sizeof(GRID_VERTICES),
	  .StrideInBytes = sizeof(XMFLOAT3),
	};

	ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &VBV);
	ID3D12GraphicsCommandList_DrawInstanced(Renderer->CommandList, GRID_VERTICES_COUNT, 1, 0, 0);

	Renderer->MeshDataOffset += CB_ALIGN(R_MeshConstants);
}