#pragma once

typedef struct R_World R_World;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

void R_CreateSceneRootSig(ID3D12Device *Device, ID3D12RootSignature **RootSign);
BOOL R_CompileSceneVS(PCWSTR FilePath, ID3DBlob **VS);
BOOL R_CompileScenePS(PCWSTR FilePath, ID3DBlob **PS);
void R_CreateScenePipelineState(R_World *Renderer);
XMMATRIX R_NormalMatrix(XMFLOAT4X4 Model);