#define CGLTF_IMPLEMENTATION
#include "../../deps/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../deps/stb_image.h"

#include "../../deps/b64.h"

#include "../core/log.h"
#include "../core/scene.h"
#include "../renderer/render_types.h"
#include "gltf.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static int load_image(cgltf_data *data, cgltf_image *img, uint8_t **outPixels, size_t *outSize, int *outW, int *outH, int *outChannels);

/****************************************************
	Public functions
*****************************************************/

BOOL SendaiGLTF_load(const char *path, Sendai_Scene *out_scene)
{
	cgltf_options options = {0};
	cgltf_data *data = NULL;

	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success) {
		Sendai_Log_appendf("cgltf_parse_file failed: %d\n", result);
		return false;
	}

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success) {
		Sendai_Log_appendf("cgltf_load_buffers failed: %d\n", result);
		cgltf_free(data);
		return false;
	}
	out_scene->meshes = malloc(data->meshes_count * sizeof(Sendai_Mesh));
	out_scene->mesh_count = data->meshes_count;
	if (data->meshes_count == 0) {
		Sendai_Log_append("No meshes in glTF\n");
		cgltf_free(data);
		return false;
	}
	size_t image_count = data->images_count || 1;

	for (size_t mesh_id = 0; mesh_id < data->meshes_count; mesh_id++) {
		out_scene->meshes[mesh_id].textures = calloc(image_count, sizeof(Sendai_Texture));
		out_scene->meshes[mesh_id].texture_count = image_count;

		for (size_t i = 0; i < image_count; ++i) {
			uint8_t *pixels = NULL;
			size_t size = 0;
			int w = 0, h = 0, channels = 0;

			int loaded = 0;
			if (i < data->images_count) {
				loaded = load_image(data, &data->images[i], &pixels, &size, &w, &h, &channels);
			}

			if (!loaded) {
				uint8_t *white = (uint8_t *)malloc(4);
				white[0] = 255;
				white[1] = 255;
				white[2] = 255;
				white[3] = 255;

				out_scene->meshes[mesh_id].textures[i].pixels = white;
				out_scene->meshes[mesh_id].textures[i].width = 1;
				out_scene->meshes[mesh_id].textures[i].height = 1;
			} else {
				out_scene->meshes[mesh_id].textures[i].pixels = pixels;
				out_scene->meshes[mesh_id].textures[i].width = w;
				out_scene->meshes[mesh_id].textures[i].height = h;
			}
		}

		cgltf_mesh *mesh = &data->meshes[0];
		cgltf_primitive *prim = &mesh->primitives[0];

		cgltf_accessor *pos_accessor = NULL;
		cgltf_accessor *color_accessor = NULL;
		cgltf_accessor *uv_accessor = NULL;

		for (cgltf_size i = 0; i < prim->attributes_count; i++) {
			cgltf_attribute *attr = &prim->attributes[i];
			switch (attr->type) {
			case cgltf_attribute_type_position:
				pos_accessor = attr->vertex_data;
				break;
			case cgltf_attribute_type_color:
				color_accessor = attr->vertex_data;
				break;
			case cgltf_attribute_type_texcoord:
				if (attr->index == 0) {
					uv_accessor = attr->vertex_data;
				}
				break;
			default:
				break;
			}
		}

		if (!pos_accessor) {
			Sendai_Log_append("Mesh has no POSITION attribute\n");
			cgltf_free(data);
			return false;
		}

		size_t vertex_count = pos_accessor->count;
		Sendai_Vertex *vertices = (Sendai_Vertex *)malloc(sizeof(Sendai_Vertex) * vertex_count);

		for (size_t i = 0; i < vertex_count; i++) {
			float pos[3];
			cgltf_accessor_read_float(pos_accessor, i, pos, 3);
			vertices[i].position = (Sendai_Float4){pos[0], pos[1], -pos[2], 1.0f};

			if (color_accessor) {
				float c[4];
				cgltf_accessor_read_float(color_accessor, i, c, 4);
				vertices[i].color.x = c[0];
				vertices[i].color.y = c[1];
				vertices[i].color.z = c[2];
				vertices[i].color.w = 1.0f;
			} else {
				vertices[i].color = (Sendai_Float4){1.0f, 1.0f, 1.0f, 1.0f};
			}

			if (uv_accessor) {
				float uv[2];
				cgltf_accessor_read_float(uv_accessor, i, uv, 2);
				vertices[i].uv.u = uv[0];
				vertices[i].uv.v = 1.0f - uv[1]; // DX Flip
			} else {
				vertices[i].uv.u = 0.0f;
				vertices[i].uv.v = 0.0f;
			}
		}

		cgltf_accessor *idx_accessor = prim->indices;
		size_t index_count = idx_accessor ? idx_accessor->count : 0;

		uint16_t *indices = NULL;
		if (index_count > 0) {
			indices = (uint16_t *)malloc(sizeof(uint16_t) * index_count);
			for (size_t i = 0; i < index_count; i++) {
				uint32_t v;
				cgltf_accessor_read_uint(idx_accessor, (int)i, &v, 1);
				indices[i] = (uint16_t)v;
			}
		}

		out_scene->meshes[mesh_id].vertices = vertices;
		out_scene->meshes[mesh_id].vertex_count = vertex_count;
		out_scene->meshes[mesh_id].indices = indices;
		out_scene->meshes[mesh_id].index_count = index_count;
	}

	cgltf_free(data);

	Sendai_Log_appendf("Successfully loaded %s\n", path);
	return true;
}

void SendaiGLTF_release(Sendai_Mesh *model)
{
	free(model->vertices);
	free(model->indices);
}

/****************************************************
	Implementation of private functions
*****************************************************/

int load_image(cgltf_data *data, cgltf_image *img, uint8_t **out_pixels, size_t *out_size, int *out_w, int *out_h, int *out_channels)
{
	if (!out_pixels || !out_size || !out_w || !out_h || !out_channels) {
		return 0;
	}

	*out_pixels = NULL;
	*out_size = 0;
	unsigned char *stbi_data = NULL;

	if (img->uri) {
		BOOL is_data_embedded = strncmp(img->uri, "data:", 5) == 0;
		if (is_data_embedded) {
			const char *comma = strchr(img->uri, ',');
			if (!comma) {
				return 0;
			}
			const char *encoded_b64 = comma + 1;
			size_t decoded_len = strlen(encoded_b64) * 3 / 4;
			char *decoded = malloc(decoded_len);
			b64_decode(encoded_b64, decoded);
			stbi_data = stbi_load_from_memory(decoded, (int)decoded_len, out_w, out_h, out_channels, 4);

			free(decoded);

			if (!stbi_data) {
				return 0;
			}
		} else {
			stbi_data = stbi_load(img->uri, out_w, out_h, out_channels, 4);
			if (!stbi_data) {
				return 0;
			}
		}
	} else if (img->buffer_view) {
		cgltf_buffer_view *buffer_view = img->buffer_view;
		cgltf_buffer *buf = buffer_view->buffer;
		const uint8_t *data = (const uint8_t *)buf->vertex_data + buffer_view->offset;
		int channels;
		unsigned char *stbi_data = stbi_load_from_memory(data, (int)buffer_view->size, out_w, out_h, out_channels, 4);
		if (stbi_data == NULL) {
			return 0;
		}
	}

	if (stbi_data == NULL) {
		return 1;
	}
	*out_size = (size_t)(*out_w) * (size_t)(*out_h) * 4;
	*out_pixels = (uint8_t *)malloc(*out_size);
	if (*out_pixels == NULL) {
		stbi_image_free(stbi_data);
		return 1;
	}
	memcpy(*out_pixels, stbi_data, *out_size);
	stbi_image_free(stbi_data);
	return 1;
}
