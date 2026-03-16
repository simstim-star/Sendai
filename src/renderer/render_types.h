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
	R_Float4 Normal;
	float UV0[2];
	float UV1[2];
} R_Vertex;

typedef struct R_PBRConstantBuffer {
	float BaseColorFactor[4];
	float UVOffset[2];
	float UVScale[2];
	float UVRotation;
	float Padding[3];
} R_PBRConstantBuffer;

typedef struct R_PointLight {
	float LightPosition[4];
	float DiffuseColor[4];
	float AmbientColor[4];
	float SpecularColor[4];
} R_Light;

typedef struct R_SceneData {
	XMFLOAT3 ViewPosition;
	R_Light Light;
	float Shininess;
	float Padding[3];
} R_SceneData;

typedef struct R_MeshConstants {
	XMFLOAT4X4 Model;
} R_MeshConstants;

#define NUM_32BITS_PBR_VALUES sizeof(R_PBRConstantBuffer) / 4

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
	INT SpecularIndex;

	int UVChannel;
	R_PBRConstantBuffer cb;

	D3D12_GPU_DESCRIPTOR_HANDLE MaterialDescriptorBase;
} R_Primitive;

typedef struct R_Mesh {
	R_Primitive *Primitives;
	UINT PrimitivesCount;

	// RH and Row-Major
	XMFLOAT4X4 ModelMatrix;
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