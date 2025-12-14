#define CGLTF_IMPLEMENTATION
#include "../../deps/cgltf/cgltf.h"

#include "gltf.h"
#include "../renderer/render_types.h"
#include "../core/log.h"

bool SendaiGLTF_load(const char *path, Sendai_Model *out_model) {
	cgltf_options options = {0};
	cgltf_data *vertex_data = NULL;
	cgltf_result result = cgltf_parse_file(&options, path, &vertex_data);

	if (result != cgltf_result_success) {
		Sendai_Log_appendf("cgltf_parse_file failed: %d\n", result); 
		return false;
	}

	result = cgltf_load_buffers(&options, vertex_data, path);
	if (result != cgltf_result_success) {
		Sendai_Log_appendf("cgltf_load_buffers failed: %d\n", result); 
		cgltf_free(vertex_data);
		return false;
	}

	result = cgltf_validate(vertex_data);
	if (result != cgltf_result_success) {
		Sendai_Log_appendf("cgltf_validate failed: %d\n", result); 
		cgltf_free(vertex_data);
		return false;
	}

	if (vertex_data->meshes_count == 0) {
		Sendai_Log_appendf("No meshes in glTF\n"); 
		cgltf_free(vertex_data);
		return false;
	}

	// using single mesh for now...
	cgltf_mesh *mesh = &vertex_data->meshes[0];
	cgltf_primitive *prim = &mesh->primitives[0];
	cgltf_accessor *pos_accessor = NULL;
	cgltf_accessor *color_accessor = NULL;
	

	for (cgltf_size i = 0; i < prim->attributes_count; i++) {
		cgltf_attribute *attr = &prim->attributes[i];
		switch (attr->type) { 
		case cgltf_attribute_type_position:
			pos_accessor = attr->vertex_data;
			break;
		case cgltf_attribute_type_color:
			color_accessor = attr->vertex_data;
			break;
		default:
			break;
		}
	}

	if (!pos_accessor) {
		Sendai_Log_append("Mesh has no POSITION attribute\n"); 
		cgltf_free(vertex_data);
		return false;
	}

	size_t vertex_count = pos_accessor->count;
	Sendai_Vertex *vertices = malloc(sizeof(Sendai_Vertex) * vertex_count);
	float *color_buf = NULL;
	float *pos_buf = malloc(sizeof(float) * vertex_count * 3);
	cgltf_accessor_unpack_floats(pos_accessor, pos_buf, pos_accessor->count * 3);

	if (color_accessor) {
		color_buf = malloc(sizeof(float) * vertex_count * 4);
		cgltf_accessor_unpack_floats(color_accessor, color_buf, color_accessor->count * 3);
	}

	for (size_t i = 0; i < vertex_count; i++) {
		vertices[i].position.x = pos_buf[i * 3 + 0];
		vertices[i].position.y = pos_buf[i * 3 + 1];
		// DirectX is left-handed: +Z extends into the screen, away from the viewer
		// glTF is right-handed: +Z extends away from the screen, into the viewew
		vertices[i].position.z = -pos_buf[i * 3 + 2];
		vertices[i].position.w = 1.0f;

		if (color_buf) {
			vertices[i].color.x = color_buf[i * 3 + 0];
			vertices[i].color.y = color_buf[i * 3 + 1];
			vertices[i].color.z = color_buf[i * 3 + 2];
			vertices[i].color.w = 1.0;
		} else {
			vertices[i].color.x = vertices[i].color.y = vertices[i].color.z = vertices[i].color.w = 1.0f;
		}
	}

	cgltf_accessor *idx_accessor = prim->indices;
	size_t index_count = (idx_accessor ? idx_accessor->count : 0);
	uint16_t *indices = NULL;
	if (index_count > 0) indices = malloc(sizeof(uint16_t) * index_count);

	if (indices && idx_accessor) {
		for (size_t i = 0; i < index_count; i++) {
			uint32_t v;
			cgltf_accessor_read_uint(idx_accessor, (int)i, &v, 1);
			indices[i] = (uint16_t)v;
		}
	}


	out_model->vertices = vertices;
	out_model->vertex_count = vertex_count;
	out_model->indices = indices;
	out_model->index_count = index_count;

	free(pos_buf);
	free(color_buf);
	cgltf_free(vertex_data);

	Sendai_Log_appendf("Succesfully loaded %s\n", path); 
	
	return true;
}

void SendaiGLTF_release(Sendai_Model *model) {
	free(model->vertices);
	free(model->indices);
}
