#pragma once

#include <windows.h>
#include <d3dcommon.h>

typedef struct Sendai_Mesh Sendai_Mesh;
typedef struct Sendai_WorldRenderer Sendai_WorldRenderer;
typedef struct Sendai_Mesh Sendai_Mesh;
typedef struct ID3D12Device ID3D12Device;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct Sendai_Scene {
	Sendai_Mesh *meshes;
	int mesh_count;

	ID3DBlob *vertex_shader;
	ID3DBlob *pixel_shader;
	ID3D12RootSignature *root_sign;
} Sendai_Scene;

void Sendai_create_scene_root_sig(ID3D12Device *device, ID3D12RootSignature **root_sign);

BOOL Sendai_compile_scene_vs(const WCHAR *file_path, ID3DBlob **vertex_shader);
BOOL Sendai_compile_scene_ps(const WCHAR *file_path, ID3DBlob **pixel_shader);
void Sendai_create_scene_pipeline_state(Sendai_WorldRenderer *renderer, Sendai_Scene *scene);
