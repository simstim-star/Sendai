// load_gltf.h

#pragma once

#include <windows.h>

typedef struct Sendai_Vertex Sendai_Vertex;
typedef struct Sendai_Texture Sendai_Texture;
typedef struct Sendai_Mesh Sendai_Mesh;

BOOL SendaiGLTF_load(const char *path, Sendai_Mesh *out_model);
void SendaiGLTF_release(Sendai_Mesh *model);