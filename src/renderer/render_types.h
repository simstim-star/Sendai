#pragma once

#include "DirectXMathC.h"

#include <d3d12.h>

typedef struct Sendai_Color {
	float r, g, b, a;
} Sendai_Color;

typedef struct Sendai_Float2 {
	float x, y;
} Sendai_Float2;

typedef struct Sendai_Float4 {
	float x, y, z, w;
} Sendai_Float4;

typedef struct Sendai_Vertex {
	Sendai_Float4 position;
	Sendai_Float4 color;
	struct {
		float u;
		float v;
	} uv;
} Sendai_Vertex;

typedef struct Sendai_ConstantBuffer {
	XMMATRIX mvp_matrix;

	// Constant buffers must be 256-byte aligned. XMMATRIX is 64 bytes.
	char padding[192];
} Sendai_ConstantBuffer;

typedef struct Sendai_Texture {
	uint8_t *pixels; // RGBA8
	int width;
	int height;
} Sendai_Texture;

typedef struct Sendai_Mesh {
	Sendai_Vertex *vertices;
	UINT vertex_count;

	UINT *indices;
	UINT index_count;

	Sendai_Texture *textures;
	UINT texture_count;
} Sendai_Mesh;

typedef struct Sendai_Model {
	char* name;
	UINT id;
	XMFLOAT3 position;
	Sendai_Mesh *meshes;
} Sendai_Model;