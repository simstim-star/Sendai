#pragma once

typedef struct SR_Color {
	float r, g, b, a;
} SR_Color;

typedef struct SR_Float3 {
	float x, y, z;
} SR_Float3;

typedef struct SR_Float4 {
	float x, y, z, w;
} SR_Float4;

typedef struct SR_Vertex {
	SR_Float3 position;
	SR_Float4 color;
} SR_Vertex;