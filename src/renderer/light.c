#include "../core/pch.h"

#include "light.h"
#include "../core/scene.h"
#include "../core/engine.h"
#include "../core/camera.h"
#include "../core/memory.h"

void
R_LightsInit(S_Scene *const Scene, const R_Camera *const Camera)
{
	Scene->ActiveLightMask = 0;
	Scene->ActiveLightMask |= (1 << 0);
	Scene->Data = (R_SceneData){.CameraPosition = Camera->Position,
								.Lights = {
								  {.LightPosition = {0.0f, 10.0f, 0.0f}, .LightColor = {300.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								  {.LightPosition = {0.0f, 0.0f, 0.0f}, .LightColor = {100.0f, 100.0f, 100.0f}},
								}};
}

void
R_UpdateLights(BYTE ActiveLightMask, const R_Light *const InLights, R_Light *const OutLights, UINT NumLights)
{
	for (UINT i = 0; i < NumLights; i++) {
		if (IS_LIGHT_ACTIVE(ActiveLightMask, i)) {
			OutLights[i].LightColor = InLights[i].LightColor;
			OutLights[i].LightPosition = InLights[i].LightPosition;
		}
	}
}

void
R_RenderLightBillboard(const R_MeshConstants *const MeshConstants, R_Core *const Renderer, XMFLOAT3 Tint, UINT SrvIndex)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignBillboard);
	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[ERS_BILLBOARD]);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	M_UpdateResourceData(Renderer->MeshDataUploadBuffer, MeshConstants, sizeof(R_MeshConstants), Renderer->MeshDataOffset);

	D3D12_GPU_VIRTUAL_ADDRESS MeshDataGpuAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->MeshDataUploadBuffer);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, MeshDataGpuAddress + Renderer->MeshDataOffset);

	UINT IncrementSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_GPU_DESCRIPTOR_HANDLE LampHandle;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &LampHandle);
	LampHandle.ptr += (UINT64)SrvIndex * IncrementSize;
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 1, LampHandle);

	struct BillboardVertex {
		XMFLOAT3 Position;
		XMFLOAT3 Color;
		XMFLOAT2 UV;
	} BillboardVertices[] = {
	  {{-0.5f, -0.5f, 0.0f}, {Tint.x, Tint.y, Tint.z}, {0.0f, 1.0f}},
	  {{-0.5f, 0.5f, 0.0f}, {Tint.x, Tint.y, Tint.z}, {0.0f, 0.0f}},
	  {{0.5f, -0.5f, 0.0f}, {Tint.x, Tint.y, Tint.z}, {1.0f, 1.0f}},
	  {{0.5f, 0.5f, 0.0f}, {Tint.x, Tint.y, Tint.z}, {1.0f, 0.0f}},
	};

	M_UpdateResourceData(Renderer->SceneDataUploadBuffer, &BillboardVertices, sizeof(BillboardVertices), Renderer->SceneDataOffset);

	D3D12_VERTEX_BUFFER_VIEW VBV = {
	  .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->SceneDataUploadBuffer) + Renderer->SceneDataOffset,
	  .SizeInBytes = sizeof(BillboardVertices),
	  .StrideInBytes = sizeof(struct BillboardVertex),
	};
	ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &VBV);

	ID3D12GraphicsCommandList_DrawInstanced(Renderer->CommandList, 4, 1, 0, 0);

	Renderer->SceneDataOffset += CB_ALIGN(BillboardVertices);
	Renderer->MeshDataOffset += CB_ALIGN(R_MeshConstants);
}