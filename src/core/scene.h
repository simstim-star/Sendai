#pragma once

#include "memory.h"
#include "../renderer/render_types.h"

typedef struct R_Model R_Model;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct S_Scene {
	R_Model *Models;
	size_t ModelsCount;
	size_t ModelsCapacity;

	R_SceneData Data;
	BYTE ActiveLightMask;

	M_Arena SceneArena;
	M_Arena TextureArena;
} S_Scene;