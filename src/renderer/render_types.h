#pragma once

#include "DirectXMathC.h"

#include <d3d12.h>

typedef struct R_Color {
	float R, G, B, A;
} R_Color;

typedef struct R_Float2 {
	float X, Y;
} R_Float2;

typedef struct R_Float4 {
	float X, Y, Z, W;
} R_Float4;

typedef struct R_Vertex {
	R_Float4 Position;
	R_Float4 Color;
	struct {
		float U;
		float V;
	} UV;
} R_Vertex;

typedef struct R_ConstantBuffer {
	XMMATRIX MVP;

	// Constant buffers must be 256-byte aligned. XMMATRIX is 64 bytes.
	char Padding[192];
} R_ConstantBuffer;

typedef struct R_Texture {
	uint8_t *Pixels; // RGBA8
	int Width;
	int Height;
	char *Name;
} R_Texture;

typedef struct R_Primitive {
	R_Vertex *Vertices;
	UINT VertexCount;
	ID3D12Resource *VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	UINT *Indices;
	UINT IndexCount;
	ID3D12Resource *IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;
} R_Primitive;

typedef struct R_Mesh {
	R_Primitive *Primitives;
	UINT PrimitivesCount;

	UINT BaseTextureIndex;
} R_Mesh;

typedef struct R_Model {
	char* Name;
	UINT Id;
	XMFLOAT3 Position;
	R_Mesh *Meshes;
	UINT MeshesCount;

	R_Texture *Images;
	UINT ImagesCount;
} R_Model;