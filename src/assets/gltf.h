// load_gltf.h

#pragma once

#include <windows.h>

typedef struct R_Vertex R_Vertex;
typedef struct R_Texture R_Texture;
typedef struct R_Mesh R_Mesh;
typedef struct SendaiScene SendaiScene;

BOOL SendaiGLTF_LoadModel(PCWSTR path, SendaiScene *out_scene);