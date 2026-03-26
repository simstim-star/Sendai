#pragma once

#include "DirectXMathC.h"

typedef struct R_Vertex {
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Tangent;
	XMFLOAT2 UV0;
	XMFLOAT2 UV1;
} R_Vertex;

typedef struct R_PBRConstantBuffer {
	XMFLOAT4 BaseColorFactor;
	FLOAT MetallicFactor;
	FLOAT RoughnessFactor;
	FLOAT Padding0[2];

	XMFLOAT3 EmissiveFactor;
	FLOAT Padding1;

	XMFLOAT2 UVOffset;
	XMFLOAT2 UVScale;
	FLOAT UVRotation;

	UINT32 AlbedoTextureIndex;
	UINT32 NormalTextureIndex;
	UINT32 MetallicTextureIndex;
	UINT32 OcclusionTextureIndex;
	UINT32 EmissiveTextureIndex;
} R_PBRConstantBuffer;

#define NUM_32BITS_PBR_VALUES sizeof(R_PBRConstantBuffer) / 4

typedef struct R_Light {
	XMFLOAT3 LightPosition;
	FLOAT Padding0;
	XMFLOAT3 LightColor;
	FLOAT Padding1;
} R_Light;

typedef struct R_SceneData {
	R_Light Lights[7];

	XMFLOAT3 CameraPosition;
	FLOAT Padding0;
} R_SceneData;

typedef struct R_MVP {
	XMMATRIX Model;
	XMMATRIX View;
	XMMATRIX Proj;
} R_MVP;

typedef struct R_MeshConstants {
	R_MVP MVP;
	XMMATRIX Normal;
} R_MeshConstants;

typedef struct R_LightBillboardConstants {
	R_MVP MVP;
	XMFLOAT3 Tint;
} R_LightBillboardConstants;

typedef struct R_Texture {
	const UINT8 *Pixels;
	INT Width;
	INT Height;
	INT Channels;
	PSTR Name;
	size_t Size;
} R_Texture;

typedef struct R_Primitive {
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	D3D12_INDEX_BUFFER_VIEW IndexBufferView;
	UINT IndexCount;

	R_PBRConstantBuffer ConstantBuffer;
} R_Primitive;

typedef struct R_Mesh {
	R_Primitive *Primitives;
	size_t PrimitivesCount;
} R_Mesh;

typedef struct R_Node {
	R_Mesh *Mesh;

	// RH and Row-Major
	XMFLOAT4X4 ModelMatrix;
} R_Node;

typedef struct R_Model {
	PSTR Name;

	R_Node *Nodes;
	size_t NodesCount;

	XMFLOAT3 Position;
	XMFLOAT3 Rotation;
	XMFLOAT3 Scale;

	R_Texture *Images;
	size_t ImagesCount;

	BOOL Visible;
} R_Model;
