// load_gltf.h

#pragma once

#include <windows.h>

typedef struct Sendai_Vertex Sendai_Vertex;
typedef struct Sendai_Texture Sendai_Texture;
typedef struct Sendai_Mesh Sendai_Mesh;
typedef struct Sendai_Scene Sendai_Scene;


BOOL SendaiGLTF_load(const char *path, Sendai_Scene *out_scene);
void SendaiGLTF_release(Sendai_Mesh *model);