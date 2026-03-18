#pragma once

#include "../core/pch.h"

#include "../core/log.h"
#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "render_types.h"
#include "renderer.h"
#include "shader.h"

void
R_CreateSceneRootSig(ID3D12Device *Device, ID3D12RootSignature **RootSign)
{
	D3D12_ROOT_PARAMETER RootParameters[4] = {0};

	// MeshData (b0)
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// PBRData (b1)
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	RootParameters[1].Constants.ShaderRegister = 1;
	RootParameters[1].Constants.Num32BitValues = NUM_32BITS_PBR_VALUES;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SceneData (b2)
	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[2].Descriptor.ShaderRegister = 2;
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Texture Table
	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 5,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
	};

	RootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[3].DescriptorTable.pDescriptorRanges = &SrvRange;
	RootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Sampler 
	D3D12_STATIC_SAMPLER_DESC Sampler = {
	  .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	  .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .ShaderRegister = 0,
	  .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};

	D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {
	  .NumParameters = _countof(RootParameters),
	  .pParameters = RootParameters,
	  .NumStaticSamplers = 1,
	  .pStaticSamplers = &Sampler,
	  .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ID3DBlob *Signature = NULL;
	ID3DBlob *Error = NULL;
	HRESULT hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error);

	if (FAILED(hr)) {
		if (Error) {
			S_LogAppend((PWSTR)ID3D10Blob_GetBufferPointer(Error));
		}
		ExitIfFailed(hr);
	}

	hr = ID3D12Device_CreateRootSignature(Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, RootSign);
	ExitIfFailed(hr);
}

HRESULT
R_CompileShader(PCWSTR FilePath, ID3DBlob **Blob, EShaderType ShaderType)
{
#if defined(_DEBUG)
	const UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT CompileFlags = 0;
#endif
	ID3DBlob *ErrorBlob = NULL;
	HRESULT hr = S_OK;

	switch (ShaderType) {
	case EST_VERTEX_SHADER:
		hr = D3DCompileFromFile(FilePath, NULL, NULL, "VSMain", "vs_5_0", CompileFlags, 0, Blob, &ErrorBlob);
		break;
	case EST_PIXEL_SHADER:
		hr = D3DCompileFromFile(FilePath, NULL, NULL, "PSMain", "ps_5_0", CompileFlags, 0, Blob, &ErrorBlob);
		break;
	}

	if (ErrorBlob) {
		OutputDebugStringA(ID3D10Blob_GetBufferPointer(ErrorBlob));
	}

	return hr;
}

void
R_CreateScenePipelineState(R_Core *Renderer)
{
	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSign,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = InputElementDescs, .NumElements = _countof(InputElementDescs)},
	  .VS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(Renderer->VS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(Renderer->VS),
		  },
	  .PS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(Renderer->PS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(Renderer->PS),
		  },
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
	  .DepthStencilState = CD3DX12_DEFAULT_DEPTH_STENCIL_DESC(),
	  .DSVFormat = DXGI_FORMAT_D32_FLOAT,
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc.Count = 1,
	};

	HRESULT hr =
		ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_GLTF]);
	ExitIfFailed(hr);

	PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState,
												  &Renderer->PipelineState[ERS_WIREFRAME]);
	ExitIfFailed(hr);
}

XMMATRIX
R_NormalMatrix(XMFLOAT4X4 Model)
{
	XMMATRIX ModelMatrix = XMLoadFloat4x4(&Model);
	XMMATRIX ModelInv = XM_MAT_INV(NULL, ModelMatrix);
	return XM_MAT_TRANSP(ModelInv);
}