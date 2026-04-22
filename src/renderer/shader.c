#pragma once

#include "core/pch.h"

#include "core/log.h"
#include "dx_helpers/desc_helpers.h"
#include "error/error.h"
#include "win32/win_path.h"
#include "render_types.h"
#include "renderer.h"
#include "shader.h"
#include "shaders/sendai/shader_defs.h"

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
		hr = D3DCompileFromFile(FilePath, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", CompileFlags, 0, Blob, &ErrorBlob);
		break;
	case EST_PIXEL_SHADER:
		hr = D3DCompileFromFile(FilePath, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", CompileFlags, 0, Blob, &ErrorBlob);
		break;
	}

	if (ErrorBlob) {
		OutputDebugStringA(ID3D10Blob_GetBufferPointer(ErrorBlob));
	}

	return hr;
}

void
R_CreatePBRPipelineState(R_Core *Renderer)
{
	D3D12_ROOT_PARAMETER RootParameters[5] = {0};

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

	// Texture Table (Bindless)
	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = PBR_N_TEXTURES_DESCRIPTORS,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 1,
	  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
	};

	RootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[3].DescriptorTable.pDescriptorRanges = &SrvRange;
	RootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE IrradianceRange = {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
											  .NumDescriptors = 1,
											  .BaseShaderRegister = 0,
											  .RegisterSpace = 0,
											  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};

	RootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[4].DescriptorTable.pDescriptorRanges = &IrradianceRange;
	RootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	D3D12_STATIC_SAMPLER_DESC Sampler = {
	  .Filter = D3D12_FILTER_ANISOTROPIC,
	  .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .MipLODBias = 0.0f,
	  .MaxAnisotropy = 16,
	  .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
	  .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
	  .MinLOD = 0.0f,			   
	  .MaxLOD = D3D12_FLOAT32_MAX,
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

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignPBR);
	ExitIfFailed(hr);

	WCHAR GLTFShadersPath[512];
	Win32FullPath(L"/shaders/sendai/bistro.hlsl", GLTFShadersPath, _countof(GLTFShadersPath));
	ID3DBlob *VS = NULL;
	hr = R_CompileShader(GLTFShadersPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);
	ID3DBlob *PS = NULL;
	hr = R_CompileShader(GLTFShadersPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignPBR,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = InputElementDescs, .NumElements = _countof(InputElementDescs)},
	  .VS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(VS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(VS),
		  },
	  .PS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(PS),
			.BytecodeLength = ID3D10Blob_GetBufferSize(PS),
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

	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_GLTF]);
	ExitIfFailed(hr);

	PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_WIREFRAME]);
	ExitIfFailed(hr);
}

void
R_CreateBillboardPipelineState(R_Core *Renderer)
{
	WCHAR LightShadersPath[512];
	Win32FullPath(L"/shaders/sendai/billboard.hlsl", LightShadersPath, _countof(LightShadersPath));
	ID3DBlob *VS = NULL;
	HRESULT hr = R_CompileShader(LightShadersPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);
	ID3DBlob *PS = NULL;
	hr = R_CompileShader(LightShadersPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	D3D12_ROOT_PARAMETER RootParameters[2];

	// MeshData (b0)
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Texture (t0)
	D3D12_DESCRIPTOR_RANGE Range = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &Range;
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
	  .NumParameters = _countof(RootParameters),
	  .pParameters = RootParameters,
	  .NumStaticSamplers = 1,
	  .pStaticSamplers = &Sampler,
	  .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ID3DBlob *Signature = NULL;
	ID3DBlob *Error = NULL;
	hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error);

	if (FAILED(hr)) {
		if (Error) {
			S_LogAppend((PWSTR)ID3D10Blob_GetBufferPointer(Error));
		}
		ExitIfFailed(hr);
	}

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignBillboard);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignBillboard,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = InputElementDescs, .NumElements = _countof(InputElementDescs)},
	  .VS = (D3D12_SHADER_BYTECODE){.pShaderBytecode = ID3D10Blob_GetBufferPointer(VS), .BytecodeLength = ID3D10Blob_GetBufferSize(VS)},
	  .PS = (D3D12_SHADER_BYTECODE){.pShaderBytecode = ID3D10Blob_GetBufferPointer(PS), .BytecodeLength = ID3D10Blob_GetBufferSize(PS)},
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .DSVFormat = DXGI_FORMAT_D32_FLOAT,
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc.Count = 1,
	};
	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	D3D12_RENDER_TARGET_BLEND_DESC TransparencyBlend = {
	  .BlendEnable = TRUE,
	  .LogicOpEnable = FALSE,
	  .SrcBlend = D3D12_BLEND_SRC_ALPHA,
	  .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
	  .BlendOp = D3D12_BLEND_OP_ADD,
	  .SrcBlendAlpha = D3D12_BLEND_ONE,
	  .DestBlendAlpha = D3D12_BLEND_ZERO,
	  .BlendOpAlpha = D3D12_BLEND_OP_ADD,
	  .LogicOp = D3D12_LOGIC_OP_NOOP,
	  .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	PSODesc.BlendState.RenderTarget[0] = TransparencyBlend;

	PSODesc.DepthStencilState = (D3D12_DEPTH_STENCIL_DESC){
	  .DepthEnable = TRUE,
	  .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
	  .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
	  .StencilEnable = FALSE,
	};

	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_BILLBOARD]);
	ExitIfFailed(hr);
}

void
R_CreateGridPipelineState(R_Core *Renderer)
{
	WCHAR ShadersPath[MAX_PATH];
	Win32FullPath(L"/shaders/sendai/grid.hlsl", ShadersPath, _countof(ShadersPath));
	ID3DBlob *VS = NULL;
	HRESULT hr = R_CompileShader(ShadersPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);
	ID3DBlob *PS = NULL;
	hr = R_CompileShader(ShadersPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	D3D12_ROOT_PARAMETER RootParameters[1];

	// MeshData (b0)
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {
	  .NumParameters = _countof(RootParameters),
	  .pParameters = RootParameters,
	  .NumStaticSamplers = 0,
	  .pStaticSamplers = NULL,
	  .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ID3DBlob *Signature = NULL;
	ID3DBlob *Error = NULL;
	hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error);

	if (FAILED(hr)) {
		if (Error) {
			S_LogAppend((PWSTR)ID3D10Blob_GetBufferPointer(Error));
		}
		ExitIfFailed(hr);
	}

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignGrid);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignGrid,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = InputElementDescs, .NumElements = _countof(InputElementDescs)},
	  .VS = (D3D12_SHADER_BYTECODE){.pShaderBytecode = ID3D10Blob_GetBufferPointer(VS), .BytecodeLength = ID3D10Blob_GetBufferSize(VS)},
	  .PS = (D3D12_SHADER_BYTECODE){.pShaderBytecode = ID3D10Blob_GetBufferPointer(PS), .BytecodeLength = ID3D10Blob_GetBufferSize(PS)},
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .DSVFormat = DXGI_FORMAT_D32_FLOAT,
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc.Count = 1,
	};
	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	D3D12_RENDER_TARGET_BLEND_DESC TransparencyBlend = {
	  .BlendEnable = TRUE,
	  .LogicOpEnable = FALSE,
	  .SrcBlend = D3D12_BLEND_SRC_ALPHA,
	  .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
	  .BlendOp = D3D12_BLEND_OP_ADD,
	  .SrcBlendAlpha = D3D12_BLEND_ONE,
	  .DestBlendAlpha = D3D12_BLEND_ZERO,
	  .BlendOpAlpha = D3D12_BLEND_OP_ADD,
	  .LogicOp = D3D12_LOGIC_OP_NOOP,
	  .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	PSODesc.BlendState.RenderTarget[0] = TransparencyBlend;

	PSODesc.DepthStencilState = (D3D12_DEPTH_STENCIL_DESC){
	  .DepthEnable = TRUE,
	  .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
	  .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
	  .StencilEnable = FALSE,
	};

	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_GRID]);
	ExitIfFailed(hr);
}

void
R_CreateCubemapPipelineState(R_Core *Renderer)
{
	D3D12_ROOT_PARAMETER RootParameters[2] = {0};

	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 1,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
	};

	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &SrvRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC Sampler = {
	  .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	  .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	  .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
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

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignCubemap);
	ExitIfFailed(hr);

	WCHAR ShaderPath[MAX_PATH];
	Win32FullPath(L"/shaders/sendai/equirect_to_cube.hlsl", ShaderPath, _countof(ShaderPath));

	ID3DBlob *VS = NULL;
	hr = R_CompileShader(ShaderPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);

	ID3DBlob *PS = NULL;
	hr = R_CompileShader(ShaderPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignCubemap,
	  .InputLayout = {InputElementDescs, _countof(InputElementDescs)},
	  .VS = {(PVOID)ID3D10Blob_GetBufferPointer(VS), ID3D10Blob_GetBufferSize(VS)},
	  .PS = {(PVOID)ID3D10Blob_GetBufferPointer(PS), ID3D10Blob_GetBufferSize(PS)},
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
	  .DepthStencilState = {.DepthEnable = FALSE, .StencilEnable = FALSE},
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT,
	  .SampleDesc = {1, 0},
	};

	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_CUBEMAP]);
	ExitIfFailed(hr);
}

void
R_CreateIrradiancePipelineState(R_Core *Renderer)
{
	D3D12_ROOT_PARAMETER RootParameters[2] = {0};

	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 1,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
	};

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

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignIrradiance);
	ExitIfFailed(hr);

	WCHAR ShaderPath[MAX_PATH];
	Win32FullPath(L"/shaders/sendai/irradiance_convolution.hlsl", ShaderPath, _countof(ShaderPath));

	ID3DBlob *VS = NULL;
	hr = R_CompileShader(ShaderPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);

	ID3DBlob *PS = NULL;
	hr = R_CompileShader(ShaderPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignIrradiance,
	  .InputLayout = {InputElementDescs, _countof(InputElementDescs)},
	  .VS = {(PVOID)ID3D10Blob_GetBufferPointer(VS), ID3D10Blob_GetBufferSize(VS)},
	  .PS = {(PVOID)ID3D10Blob_GetBufferPointer(PS), ID3D10Blob_GetBufferSize(PS)},
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
	  .DepthStencilState = {.DepthEnable = FALSE, .StencilEnable = FALSE},
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT,
	  .SampleDesc = {1, 0},
	};

	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	hr =
		ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_IRRADIANCE]);
	ExitIfFailed(hr);
}

void
R_CreateSkyboxPipelineState(R_Core *Renderer)
{
	D3D12_ROOT_PARAMETER RootParameters[2] = {0};

	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_DESCRIPTOR_RANGE SrvRange = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 1,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
	};

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

	hr = ID3D12Device_CreateRootSignature(Renderer->Device, 0, ID3D10Blob_GetBufferPointer(Signature), ID3D10Blob_GetBufferSize(Signature),
										  &IID_ID3D12RootSignature, &Renderer->RootSignSkybox);
	ExitIfFailed(hr);

	WCHAR ShaderPath[MAX_PATH];
	Win32FullPath(L"/shaders/sendai/skybox.hlsl", ShaderPath, _countof(ShaderPath));

	ID3DBlob *VS = NULL;
	hr = R_CompileShader(ShaderPath, &VS, EST_VERTEX_SHADER);
	ExitIfFailed(hr);

	ID3DBlob *PS = NULL;
	hr = R_CompileShader(ShaderPath, &PS, EST_PIXEL_SHADER);
	ExitIfFailed(hr);

	const D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {
	  .pRootSignature = Renderer->RootSignSkybox,
	  .InputLayout = {InputElementDescs, _countof(InputElementDescs)},
	  .VS = {(PVOID)ID3D10Blob_GetBufferPointer(VS), ID3D10Blob_GetBufferSize(VS)},
	  .PS = {(PVOID)ID3D10Blob_GetBufferPointer(PS), ID3D10Blob_GetBufferSize(PS)},
	  .RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
	  .BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
	  .DepthStencilState = CD3DX12_DEFAULT_DEPTH_STENCIL_DESC(),
	  .DSVFormat = DXGI_FORMAT_D32_FLOAT,
	  .SampleMask = UINT_MAX,
	  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	  .NumRenderTargets = 1,
	  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc = {1, 0},
	};

	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	hr = ID3D12Device_CreateGraphicsPipelineState(Renderer->Device, &PSODesc, &IID_ID3D12PipelineState, &Renderer->PipelineState[ERS_SKYBOX]);
	ExitIfFailed(hr);
}

XMMATRIX
R_NormalMatrix(XMFLOAT4X4 *Model)
{
	XMMATRIX ModelMatrix = XMLoadFloat4x4(Model);
	XMMATRIX ModelInv = XM_MAT_INV(NULL, ModelMatrix);
	return XM_MAT_TRANSP(ModelInv);
}