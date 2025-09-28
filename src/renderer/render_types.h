#pragma once

typedef struct snr_color_t {
	float r, g, b, a;
} snr_color_t;

typedef struct float3 {
	float x, y, z;
} float3;

typedef struct float4 {
	float x, y, z, w;
} float4;

typedef struct snr_vertex_t {
	float3 position;
	float4 color;
} snr_vertex_t;