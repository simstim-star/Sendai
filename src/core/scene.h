#pragma once

#include <windows.h>
#include <d3dcommon.h>

#include "arena.h"

typedef struct R_World R_World;
typedef struct R_Mesh R_Mesh;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct SendaiScene {
	R_Mesh *Meshes;
	int MeshCount;

	ID3DBlob *VS;
	ID3DBlob *PS;
	ID3D12RootSignature *RootSign;

	S_Arena SceneArena;
} SendaiScene;

void CreateSceneRootSig(ID3D12Device *device, ID3D12RootSignature **root_sign);

BOOL CompileSceneVS(const WCHAR *file_path, ID3DBlob **vertex_shader);
BOOL CompileScenePS(const WCHAR *file_path, ID3DBlob **pixel_shader);
void CreateScenePipelineState(R_World *renderer, SendaiScene *scene);
