#pragma once

#include "arena.h"

typedef struct R_Model R_Model;
typedef struct ID3D12RootSignature ID3D12RootSignature;

typedef struct S_Scene {
	R_Model *Models;
	UINT ModelsCount;
	UINT ModelsCapacity;

	S_Arena SceneArena;
} S_Scene;