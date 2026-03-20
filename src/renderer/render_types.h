#pragma once

#include "DirectXMathC.h"

typedef struct R_Vertex {
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 UV0;
	XMFLOAT2 UV1;
} R_Vertex;

typedef struct R_PBRConstantBuffer {
	XMFLOAT4 BaseColorFactor;
	FLOAT MetallicFactor;
	FLOAT RoughnessFactor;

	XMFLOAT2 UVOffset;
	XMFLOAT2 UVScale;
	FLOAT UVRotation;

	UINT32 AlbedoTextureIndex;
	UINT32 NormalTextureIndex;
	UINT32 MetallicTextureIndex;
	UINT32 RoughnessTextureIndex;
	UINT32 OcclusionTextureIndex;
} R_PBRConstantBuffer;

#define NUM_32BITS_PBR_VALUES sizeof(R_PBRConstantBuffer) / 4

typedef struct R_SceneData {
	XMFLOAT3 LightPosition;
	float Padding0;

	XMFLOAT3 LightColor;
	float Padding1;

	XMFLOAT3 CameraPosition;
	float Padding2;
} R_SceneData;

typedef struct R_MeshConstants {
	XMMATRIX Model;
	XMMATRIX View;
	XMMATRIX Proj;
	XMMATRIX Normal;
} R_MeshConstants;

typedef struct R_Texture {
	UINT8 *Pixels;
	INT Width;
	INT Height;
	PSTR Name;
} R_Texture;

typedef struct R_Primitive {
	R_Vertex *Vertices;
	UINT VertexCount;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	UINT *Indices;
	UINT IndexCount;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	R_Texture *Albedo;
	R_Texture *Normal;
	R_Texture *Metallic;
	R_Texture *Roughness;
	R_Texture *Occlusion;

	INT UVChannel;
	R_PBRConstantBuffer cb;
} R_Primitive;

typedef struct R_Mesh {
	R_Primitive *Primitives;
	UINT PrimitivesCount;

	// RH and Row-Major
	XMFLOAT4X4 ModelMatrix;
} R_Mesh;

typedef struct R_Model {
	PSTR Name;
	UINT Id;
	XMFLOAT3 Position;
	R_Mesh *Meshes;
	UINT MeshesCount;

	R_Texture *Images;
	UINT ImagesCount;
} R_Model;

