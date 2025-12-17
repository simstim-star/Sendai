// load_gltf.h

#pragma once

#include <windows.h>

typedef struct Sendai_Vertex Sendai_Vertex;
typedef struct Sendai_Texture Sendai_Texture;

typedef struct Sendai_Model {
	Sendai_Vertex *vertices;
	UINT vertex_count;

	UINT *indices;
	UINT index_count;

	Sendai_Texture *textures;
	UINT texture_count;
} Sendai_Model;

BOOL SendaiGLTF_load(const char *path, Sendai_Model *out_model);
void SendaiGLTF_release(Sendai_Model *model);