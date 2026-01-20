#pragma once

typedef struct Sendai_Mesh Sendai_Mesh;

typedef struct Sendai_Scene {
	Sendai_Mesh *meshes;
	int mesh_count;
} Sendai_Scene;