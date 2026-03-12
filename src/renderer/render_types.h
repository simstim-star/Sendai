#pragma once

#include "../core/pch.h"
#include "DirectXMathC.h"

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

typedef struct R_TransformConstantBuffer {
	XMMATRIX MVP;

	// Constant buffers must be 256-byte aligned. XMMATRIX is 64 bytes.
	char Padding[192];
} R_TransformBuffer;

typedef struct R_PBRConstantBuffer {
	float BaseColorFactor[4]; 
	float UVTransform[4];	  
} R_PBRConstantBuffer;

#define NUM_32BITS_PBR_VALUES sizeof(R_PBRConstantBuffer) / 2

typedef struct R_Texture {
	uint8_t *Pixels; // RGBA8
	int Width;
	int Height;
	char *Name;
} R_Texture;

typedef struct R_Primitive {
	R_Vertex *Vertices;
	UINT VertexCount;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	UINT *Indices;
	UINT IndexCount;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	INT AlbedoIndex;
	float BaseColorFactor[4]; 
	float UVScale[2]; 
	float UVOffset[2]; 

	D3D12_GPU_DESCRIPTOR_HANDLE MaterialDescriptorBase;
} R_Primitive;

typedef struct R_Mesh {
	R_Primitive *Primitives;
	UINT PrimitivesCount;

	// RH and Row-Major
	XMMATRIX ModelMatrix;
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