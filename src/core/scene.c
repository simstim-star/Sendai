#pragma once

#define COBJMACROS

#include "scene.h"
#include "../error/error.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include "../renderer/renderer.h"
#include "../dx_helpers/desc_helpers.h"

void Sendai_create_scene_root_sig(ID3D12Device *device, ID3D12RootSignature **root_sign)
{
	D3D12_DESCRIPTOR_RANGE srv_range = {
	  .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
	  .NumDescriptors = 1,
	  .BaseShaderRegister = 0,
	  .RegisterSpace = 0,
	  .OffsetInDescriptorsFromTableStart = 0,
	};

	D3D12_ROOT_PARAMETER root_parameters[2] = {0};

	/* b0 : MVP */
	root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[0].Descriptor.ShaderRegister = 0;
	root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	/* t0 : texture */
	root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[1].DescriptorTable.pDescriptorRanges = &srv_range;
	root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sampler = {
	  .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	  .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	  .ShaderRegister = 0,
	  .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {
	  .NumParameters = 2,
	  .pParameters = root_parameters,
	  .NumStaticSamplers = 1,
	  .pStaticSamplers = &sampler,
	  .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ID3DBlob *signature = NULL;
	ID3DBlob *error = NULL;
	HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	exit_if_failed(hr);

	const LPVOID buffer_ptr = ID3D10Blob_GetBufferPointer(signature);
	const SIZE_T buffer_size = ID3D10Blob_GetBufferSize(signature);
	hr = ID3D12Device_CreateRootSignature(device, 0, buffer_ptr, buffer_size, &IID_ID3D12RootSignature, root_sign);
	exit_if_failed(hr);
}

BOOL Sendai_compile_scene_vs(const WCHAR *file_path, ID3DBlob **vertex_shader)
{
#if defined(_DEBUG)
	const UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT compile_flags = 0;
#endif
	HRESULT hr = D3DCompileFromFile(file_path, NULL, NULL, "VSMain", "vs_5_0", compile_flags, 0, vertex_shader, NULL);
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL Sendai_compile_scene_ps(const WCHAR *file_path, ID3DBlob **pixel_shader)
{
#if defined(_DEBUG)
	const UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT compile_flags = 0;
#endif
	HRESULT hr = D3DCompileFromFile(file_path, NULL, NULL, "PSMain", "ps_5_0", compile_flags, 0, pixel_shader, NULL);
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

void Sendai_create_scene_pipeline_state(Sendai_WorldRenderer *renderer, Sendai_Scene *scene)
{
	const D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
	  {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
	  .pRootSignature = scene->root_sign,
	  .InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = input_element_descs, .NumElements = _countof(input_element_descs)},
	  .VS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(scene->vertex_shader),
			.BytecodeLength = ID3D10Blob_GetBufferSize(scene->vertex_shader),
		  },
	  .PS =
		  (D3D12_SHADER_BYTECODE){
			.pShaderBytecode = ID3D10Blob_GetBufferPointer(scene->pixel_shader),
			.BytecodeLength = ID3D10Blob_GetBufferSize(scene->pixel_shader),
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

	HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(renderer->device, &pso_desc, &IID_ID3D12PipelineState, &renderer->pipeline_state_scene);
	exit_if_failed(hr);
}
