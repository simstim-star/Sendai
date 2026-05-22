#pragma once

#include "DirectXMathC.h"

typedef struct BillboardVertex {
	XMFLOAT3 Position;
	XMFLOAT2 UV;
} BillboardVertex;

extern const BillboardVertex BILLBOARD_VERTICES[4];