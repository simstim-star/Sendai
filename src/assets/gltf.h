// load_gltf.h

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <windows.h>

typedef struct Sendai_Vertex Sendai_Vertex;

typedef struct Sendai_Model {
	Sendai_Vertex *vertices;
	size_t vertex_count;

	uint16_t *indices;
	UINT index_count;
} Sendai_Model;

bool SendaiGLTF_load(const char *path, Sendai_Model *out_model);
void SendaiGLTF_release(Sendai_Model *model);