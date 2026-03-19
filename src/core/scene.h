#pragma once

#include "arena.h"
#include "../renderer/render_types.h"

typedef struct R_Model R_Model;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct S_Scene {
	R_Model *Models;
	UINT ModelsCount;
	UINT ModelsCapacity;

	R_SceneData Data;

	S_Arena SceneArena;
} S_Scene;