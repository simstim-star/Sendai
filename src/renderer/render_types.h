#pragma once

#include "DirectXMathC.h"

typedef struct Sendai_Color {
	float r, g, b, a;
} Sendai_Color;

typedef struct Sendai_Float4 {
	float x, y, z, w;
} Sendai_Float4;

typedef struct Sendai_Vertex {
	Sendai_Float4 position;
	Sendai_Float4 color;
} Sendai_Vertex;

typedef struct Sendai_ConstantBuffer {
	XMMATRIX mvp_matrix;

	// Constant buffers must be 256-byte aligned. XMMATRIX is 64 bytes.
	char padding[192];
} Sendai_ConstantBuffer;