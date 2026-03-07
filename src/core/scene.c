#pragma once

#define COBJMACROS

#include "scene.h"
#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../renderer/renderer.h"
#include <d3d12.h>
#include <d3dcompiler.h>

void CreateSceneRootSig(ID3D12Device *Device, ID3D12RootSignature **RootSign)
{
	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 1,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = 0,
	};

	D3D12_ROOT_PARAMETER RootParameters[2] = {0};

	/* b0 : MVP */
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	/* t0 : texture */
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &SrvRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC Sampler = {
	  .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	  .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .ShaderRegister = 0,
	  .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};

	D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {
	  .NumParameters = 2,
	  .pParameters = RootParameters,
	  .NumStaticSamplers = 1,
	  .pStaticSamplers = &Sampler,
	  .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ID3DBlob *Signature = NULL;
	ID3DBlob *Error = NULL;
	HRESULT hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error);
	ExitIfFailed(hr);

	const LPVOID pBuffer = ID3D10Blob_GetBufferPointer(Signature);
	const SIZE_T BufferSize = ID3D10Blob_GetBufferSize(Signature);
	hr = ID3D12Device_CreateRootSignature(Device, 0, pBuffer, BufferSize, &IID_ID3D12RootSignature, RootSign);
	ExitIfFailed(hr);
}

BOOL CompileSceneVS(PCWSTR FilePath, ID3DBlob **VS)
{
#if defined(_DEBUG)
	const UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT CompileFlags = 0;
#endif
	HRESULT hr = D3DCompileFromFile(FilePath, NULL, NULL, "VSMain", "vs_5_0", CompileFlags, 0, VS, NULL);
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL CompileScenePS(PCWSTR FilePath, ID3DBlob **PS)
{
#if defined(_DEBUG)
	const UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT CompileFlags = 0;
#endif
	HRESULT hr = D3DCompileFromFile(FilePath, NULL, NULL, "PSMain", "ps_5_0", CompileFlags, 0, PS, NULL);
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

void CreateScenePipelineState(R_World *renderer, SendaiScene *scene)
{
	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = scene->RootSign,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = InputElementDescs, .NumElements = _countof(InputElementDescs)},
	  .VS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(scene->VS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(scene->VS),
		  },
	  .PS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(scene->PS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(scene->PS),
		  },
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
	  .DepthStencilState.DepthEnable = FALSE,
	  .DepthStencilState.StencilEnable = FALSE,
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc.Count = 1,
	};

	HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &renderer->PipelineStateScene);
	ExitIfFailed(hr);
}
