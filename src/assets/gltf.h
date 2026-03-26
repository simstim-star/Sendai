#pragma once

typedef struct S_Scene S_Scene;
typedef struct R_Core R_Core;
typedef struct R_Model R_Model;

BOOL SendaiGLTF_LoadModel(R_Core *Renderer, PCWSTR Path, S_Scene *Scene);