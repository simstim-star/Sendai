#pragma once

typedef struct R_Core R_Core;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef enum EShaderType {
	EST_VERTEX_SHADER,
	EST_PIXEL_SHADER,
} EShaderType;

void R_CreateSceneRootSig(ID3D12Device *Device, ID3D12RootSignature **RootSign);
HRESULT R_CompileShader(PCWSTR FilePath, ID3DBlob **Blob, EShaderType ShaderType);
void R_CreateScenePipelineState(R_Core *Renderer);
XMMATRIX R_NormalMatrix(XMFLOAT4X4 Model);