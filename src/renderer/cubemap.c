#include "core/pch.h"

#include "core/memory.h"
#include "cubemap.h"
#include "dx_helpers/desc_helpers.h"
#include "error/error.h"
#include "renderer.h"
#include <DirectXMathC.h>

#define N_CUBE_FACES 6

static const FLOAT CUBEMAP_CLEAR_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
static const FLOAT RIGHT_ANGLE_RAD = 1.5708;

static XMMATRIX CAPTURE_VIEWS[N_CUBE_FACES];

static const XMFLOAT3 LOOK_AT_CUBE_FACES[N_CUBE_FACES] = {
  {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, /* +X, -X */
  {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, /* +Y, -Y */
  {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}  /* +Z, -Z */
};

static const XMFLOAT3 UP_DIRECTION_CUBE_FACES[N_CUBE_FACES] = {
  {0.0f, 1.0f, 0.0f},  {0.0f, 1.0f, 0.0f}, /* +X, -X */
  {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, /* +Y, -Y */
  {0.0f, 1.0f, 0.0f},  {0.0f, 1.0f, 0.0f}  /* +Z, -Z */
};

static const FLOAT CUBEMAP_VERTICES[] = {
  -1.0f, 1.0f,	-1.0f, /* 0 */
  1.0f,	 1.0f,	-1.0f, /* 1 */
  1.0f,	 -1.0f, -1.0f, /* 2 */
  -1.0f, -1.0f, -1.0f, /* 3 */
  -1.0f, 1.0f,	1.0f,  /* 4 */
  1.0f,	 1.0f,	1.0f,  /* 5 */
  1.0f,	 -1.0f, 1.0f,  /* 6 */
  -1.0f, -1.0f, 1.0f   /* 7 */
};

static const UINT16 CUBEMAP_INDICES[] = {
  0, 1, 2, 0, 2, 3, /* Front */
  4, 6, 5, 4, 7, 6, /* Back */
  4, 5, 1, 4, 1, 0, /* Top */
  3, 2, 6, 3, 6, 7, /* Bottom */
  1, 5, 6, 1, 6, 2, /* Right */
  4, 0, 3, 4, 3, 7	/* Left */
};

VOID
R_SetupCubemapResources(R_Core *Renderer, R_Cubemap *const Cubemap)
{
	const DXGI_FORMAT TextureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	D3D12_RESOURCE_DESC TextureDesc = {0};
	TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TextureDesc.Width = 512;
	TextureDesc.Height = 512;
	TextureDesc.DepthOrArraySize = N_CUBE_FACES;
	TextureDesc.MipLevels = 1;
	TextureDesc.Format = TextureFormat;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	const D3D12_HEAP_PROPERTIES DefaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_CLEAR_VALUE OptimizedClear = {
	  .Format = TextureFormat,
	  .Color[0] = CUBEMAP_CLEAR_COLOR[0],
	  .Color[1] = CUBEMAP_CLEAR_COLOR[1],
	  .Color[2] = CUBEMAP_CLEAR_COLOR[2],
	  .Color[3] = CUBEMAP_CLEAR_COLOR[3],
	};
	HRESULT hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &TextureDesc,
													  D3D12_RESOURCE_STATE_COPY_DEST, &OptimizedClear, &IID_ID3D12Resource, &Cubemap->Resource);
	ExitIfFailed(hr);

	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {0};
	RTVHeapDesc.NumDescriptors = N_CUBE_FACES;
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	hr = ID3D12Device_CreateDescriptorHeap(Renderer->Device, &RTVHeapDesc, &IID_ID3D12DescriptorHeap, &Cubemap->DescriptorHeap);
	ExitIfFailed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Cubemap->DescriptorHeap, &RTVHeapHandle);
	Cubemap->RTVDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	/* Create each RTV on the RTV Heap */
	for (int Face = 0; Face < N_CUBE_FACES; Face++) {
		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {
		  .Format = TextureDesc.Format,
		  .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
		  .Texture2DArray.MipSlice = 0,
		  .Texture2DArray.FirstArraySlice = Face,
		  .Texture2DArray.ArraySize = 1,
		};
		ID3D12Device_CreateRenderTargetView(Renderer->Device, Cubemap->Resource, &RTVDesc, RTVHeapHandle);
		RTVHeapHandle.ptr += Cubemap->RTVDescriptorSize;
	}

	XMVECTOR EyePosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	for (int Face = 0; Face < N_CUBE_FACES; Face++) {
		XMVECTOR TargetDirection = XMLoadFloat3(&LOOK_AT_CUBE_FACES[Face]);
		XMVECTOR UpDirection = XMLoadFloat3(&UP_DIRECTION_CUBE_FACES[Face]);
		CAPTURE_VIEWS[Face] = XM_MAT_LOOK_AT_LH(EyePosition, TargetDirection, UpDirection);
	}

	UINT64 Alignment = CB_ALIGN(XMMATRIX[3]);
	UINT CBSize = Alignment * N_CUBE_FACES;
	D3D12_HEAP_PROPERTIES UploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC CBDesc = CD3DX12_RESOURCE_DESC_BUFFER(CBSize, D3D12_RESOURCE_FLAG_NONE, 0);

	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeapProps, D3D12_HEAP_FLAG_NONE, &CBDesc,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &Cubemap->ConstantBufferUploadHeap);

	ID3D12Resource_Map(Cubemap->ConstantBufferUploadHeap, 0, NULL, (void **)&Cubemap->MappedCBVData);
	D3D12_HEAP_PROPERTIES UploadHeap = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC_BUFFER(sizeof(CUBEMAP_VERTICES), D3D12_RESOURCE_FLAG_NONE, 0);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
											  NULL, &IID_ID3D12Resource, &Cubemap->VertexBuffer);
	ExitIfFailed(hr);

	UINT8 *pVertexData;
	ID3D12Resource_Map(Cubemap->VertexBuffer, 0, NULL, (void **)&pVertexData);
	memcpy(pVertexData, CUBEMAP_VERTICES, sizeof(CUBEMAP_VERTICES));
	ID3D12Resource_Unmap(Cubemap->VertexBuffer, 0, NULL);

	D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC_BUFFER(sizeof(CUBEMAP_INDICES), D3D12_RESOURCE_FLAG_NONE, 0);
	hr = ID3D12Device_CreateCommittedResource(Renderer->Device, &UploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
											  NULL, &IID_ID3D12Resource, &Cubemap->IndexBuffer);

	UINT8 *pIndexData;
	ID3D12Resource_Map(Cubemap->IndexBuffer, 0, NULL, (void **)&pIndexData);
	memcpy(pIndexData, CUBEMAP_INDICES, sizeof(CUBEMAP_INDICES));
	ID3D12Resource_Unmap(Cubemap->IndexBuffer, 0, NULL);

	Cubemap->CubeVBView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Cubemap->VertexBuffer);
	Cubemap->CubeVBView.StrideInBytes = sizeof(float) * 3;
	Cubemap->CubeVBView.SizeInBytes = sizeof(CUBEMAP_VERTICES);

	Cubemap->CubeIBView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Cubemap->IndexBuffer);
	Cubemap->CubeIBView.Format = DXGI_FORMAT_R16_UINT;
	Cubemap->CubeIBView.SizeInBytes = sizeof(CUBEMAP_INDICES);

	D3D12_SHADER_RESOURCE_VIEW_DESC CubemapSrvDesc = {.Format = TextureDesc.Format,
													  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
													  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
													  .TextureCube.MipLevels = 1,
													  .TextureCube.MostDetailedMip = 0,
													  .TextureCube.ResourceMinLODClamp = 0.0f};
	UINT SlotIndex = Renderer->TexturesCount++;
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &CpuHandle);
	CpuHandle.ptr += (SIZE_T)SlotIndex * Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	ID3D12Device_CreateShaderResourceView(Renderer->Device, Cubemap->Resource, &CubemapSrvDesc, CpuHandle);
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &Cubemap->GpuSrvHandle);
	Cubemap->GpuSrvHandle.ptr += (SIZE_T)SlotIndex * Renderer->DescriptorHandleIncrementSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

VOID
R_RenderCubemapOnRTVs(R_Core *Renderer, D3D12_GPU_DESCRIPTOR_HANDLE EquirectangularSRV)
{
	D3D12_RESOURCE_BARRIER BarrierFromCopyToRT = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
												  .Transition = {
													.pResource = Renderer->Cubemap.Resource,
													.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
													.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
													.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
												  }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierFromCopyToRT);

	struct {
		XMMATRIX View;
		XMMATRIX Proj;
	} EquirectToCubeCB;
	EquirectToCubeCB.Proj = XMMatrixPerspectiveFovLH(RIGHT_ANGLE_RAD, 1.0f, 0.1f, 10.0f);
	UINT64 Alignment = CB_ALIGN(EquirectToCubeCB);

	D3D12_VIEWPORT Viewport = {0.0f, 0.0f, 512.0f, 512.0f, 0.0f, 1.0f};
	D3D12_RECT Scissor = {0, 0, 512, 512};
	ID3D12GraphicsCommandList_RSSetViewports(Renderer->CommandList, 1, &Viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(Renderer->CommandList, 1, &Scissor);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(Renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_IASetVertexBuffers(Renderer->CommandList, 0, 1, &Renderer->Cubemap.CubeVBView);
	ID3D12GraphicsCommandList_IASetIndexBuffer(Renderer->CommandList, &Renderer->Cubemap.CubeIBView);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(Renderer->CommandList, Renderer->RootSignCubemap);
	ID3D12GraphicsCommandList_SetPipelineState(Renderer->CommandList, Renderer->PipelineState[ERS_CUBEMAP]);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(Renderer->CommandList, 1, EquirectangularSRV);

	D3D12_GPU_VIRTUAL_ADDRESS CBUploadHeapGPUAddress = ID3D12Resource_GetGPUVirtualAddress(Renderer->Cubemap.ConstantBufferUploadHeap);
	for (int i = 0; i < N_CUBE_FACES; i++) {
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentFaceHandle;
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(Renderer->Cubemap.DescriptorHeap, &CurrentFaceHandle);
		CurrentFaceHandle.ptr += (i * Renderer->Cubemap.RTVDescriptorSize);
		ID3D12GraphicsCommandList_OMSetRenderTargets(Renderer->CommandList, 1, &CurrentFaceHandle, FALSE, NULL);
		ID3D12GraphicsCommandList_ClearRenderTargetView(Renderer->CommandList, CurrentFaceHandle, ((float[4]){0.0f, 0.0f, 0.0f, 1.0f}), 0, NULL);

		EquirectToCubeCB.View = CAPTURE_VIEWS[i];
		memcpy(Renderer->Cubemap.MappedCBVData + (i * Alignment), &EquirectToCubeCB, sizeof(EquirectToCubeCB));

		ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(Renderer->CommandList, 0, CBUploadHeapGPUAddress + (i * Alignment));
		ID3D12GraphicsCommandList_DrawIndexedInstanced(Renderer->CommandList, 36, 1, 0, 0, 0);
	}

	D3D12_RESOURCE_BARRIER BarrierFromRTToPixelShaderResource = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
																 .Transition = {
																   .pResource = Renderer->Cubemap.Resource,
																   .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
																   .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
																   .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																 }};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierFromRTToPixelShaderResource);
}

VOID
R_DrawSkybox(R_Core *Renderer, XMMATRIX View, XMMATRIX Proj)
{
	struct {
		XMMATRIX View;
		XMMATRIX Proj;
	} SkyboxCB;

	SkyboxCB.View = View;
	SkyboxCB.View.r[3] = XMVectorSet(0, 0, 0, 1);
	SkyboxCB.Proj = Proj;

	UINT64 Size = CB_ALIGN(sizeof(SkyboxCB));
	BYTE *Dest = Renderer->SceneDataUploadBufferCpuAddress + Renderer->SceneDataOffset;
	memcpy(Dest, &SkyboxCB, sizeof(SkyboxCB));

	ID3D12GraphicsCommandList *CommandList = Renderer->CommandList;
	ID3D12DescriptorHeap *Heaps[] = {Renderer->TexturesHeap};
	D3D12_GPU_VIRTUAL_ADDRESS CBAddress = M_GpuAddress(Renderer->SceneDataUploadBuffer, Renderer->SceneDataOffset);
	Renderer->SceneDataOffset += Size;

	ID3D12GraphicsCommandList_SetDescriptorHeaps(Renderer->CommandList, _countof(Heaps), Heaps);
	ID3D12GraphicsCommandList_SetPipelineState(CommandList, Renderer->PipelineState[ERS_SKYBOX]);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(CommandList, Renderer->RootSignSkybox);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(CommandList, 0, CBAddress);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(CommandList, 1, Renderer->Cubemap.GpuSrvHandle);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_IASetVertexBuffers(CommandList, 0, 1, &Renderer->Cubemap.CubeVBView);
	ID3D12GraphicsCommandList_IASetIndexBuffer(CommandList, &Renderer->Cubemap.CubeIBView);
	ID3D12GraphicsCommandList_DrawIndexedInstanced(CommandList, 36, 1, 0, 0, 0);
}