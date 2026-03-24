#pragma once

#include "DirectXMathC.h"

typedef struct BillboardVertex {
	XMFLOAT3 Position;
	XMFLOAT2 UV;
} BillboardVertex;

extern BillboardVertex BillboardVertices[4];