#pragma once

typedef struct R_Core R_Core;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef enum EShaderType {
	EST_VERTEX_SHADER,
	EST_PIXEL_SHADER,
} EShaderType;

HRESULT R_CompileShader(PCWSTR FilePath, ID3DBlob **Blob, EShaderType ShaderType);
void R_CreatePBRPipelineState(R_Core *Renderer);
void R_CreateBillboardPipelineState(R_Core *Renderer);
XMMATRIX R_NormalMatrix(XMFLOAT4X4 *Model);
