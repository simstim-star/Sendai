#pragma once

#include "arena.h"

typedef struct R_World R_World;
typedef struct R_Model R_Model;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct SendaiScene {
	R_Model *Models;
	UINT ModelsCount;
	UINT ModelsCapacity;

	ID3DBlob *VS;
	ID3DBlob *PS;
	ID3D12RootSignature *RootSign;

	S_Arena SceneArena;
} SendaiScene;

void CreateSceneRootSig(ID3D12Device *Device, ID3D12RootSignature **RootSign);

BOOL CompileSceneVS(PCWSTR FilePath, ID3DBlob **VS);
BOOL CompileScenePS(PCWSTR FilePath, ID3DBlob **PS);
void CreateScenePipelineState(R_World *Renderer, SendaiScene *Scene);
