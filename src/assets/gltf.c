#include "../core/pch.h"

#define CGLTF_IMPLEMENTATION
#include "../../deps/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "../../deps/stb_image.h"

#include "../../deps/b64.h"

#include "../core/log.h"
#include "../core/scene.h"
#include "../renderer/render_types.h"
#include "gltf.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void PreloadImages(S_Scene *Scene, cgltf_data *Data, PCWSTR Path);

static BOOL ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH],
							 _In_ cgltf_image *Img,
							 _Outptr_result_bytebuffer_(*Size) UINT8 **Pixels,
							 _Out_ size_t *Size,
							 _Out_ int *W,
							 _Out_ int *H,
							 _Out_ int *Channels);

static void RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH]);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _In_ S_Arena *Arena, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffers(_In_ const cgltf_options *Options, _In_ S_Arena *Arena, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path);

static cgltf_result LoadGLTFBuffer(
	_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _In_ S_Arena *Arena, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data);

static void AppendFileNameToPath(_In_z_ PWSTR BasePath, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH]);

static int IsDataEmbedded(const cgltf_image *const BaseImage);

// The below functions are to inject into gltf to use my arena
static void *cgltf_arena_alloc(void *user, cgltf_size size);
static void cgltf_arena_free(void *user, void *ptr);

/****************************************************
	Public functions
*****************************************************/

BOOL
SendaiGLTF_LoadModel(PCWSTR Path, S_Scene *Scene)
{
	S_Arena LocalArena = S_ArenaInit(GIGABYTES(1));

	void *FileData = NULL;
	LONG Size = LoadGLTFFile(Path, &LocalArena, &FileData);
	if (Size <= 0) {
		S_LogAppendf(L"Failed to load %s\n", Path);
		return FALSE;
	}

	cgltf_options Options = {0};
	Options.memory.alloc_func = cgltf_arena_alloc;
	Options.memory.free_func = cgltf_arena_free;
	Options.memory.user_data = &LocalArena;

	cgltf_data *Data = NULL;
	cgltf_result Result = cgltf_parse(&Options, FileData, Size, &Data);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to parse %s\n", Path);
		return FALSE;
	}

	Result = LoadGLTFBuffers(&Options, &LocalArena, Data, Path);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to load GLTF buffers from %s\n", Path);
		return FALSE;
	}

	if (Data->meshes_count == 0) {
		S_LogAppend(L"No meshes in glTF\n");
		cgltf_free(Data);
		return FALSE;
	}

	Scene->Models[Scene->ModelsCount].Meshes = S_ArenaAlloc(&Scene->SceneArena, Data->meshes_count * sizeof(R_Mesh));
	Scene->Models[Scene->ModelsCount].MeshesCount = Data->meshes_count;

	if (Data->images_count > 0) {
		PreloadImages(Scene, Data, Path);
	}

	size_t NodeCount = Data->nodes_count;
	Scene->Models[Scene->ModelsCount].Meshes = S_ArenaAlloc(&Scene->SceneArena, NodeCount * sizeof(R_Mesh));
	Scene->Models[Scene->ModelsCount].MeshesCount = 0;

	for (size_t i = 0; i < NodeCount; i++) {
		cgltf_node *NodeData = &Data->nodes[i];
		if (NodeData->mesh == NULL) {
			continue;
		}

		R_Mesh *Mesh = &Scene->Models[Scene->ModelsCount].Meshes[Scene->Models[Scene->ModelsCount].MeshesCount];
		cgltf_mesh *MeshData = NodeData->mesh;

		float TransformColMajor[4][4];
		cgltf_node_transform_world(NodeData, TransformColMajor);

		// Note: Mesh->Transform is col-major, but XMLoadFloat4x4 expects row-major.
		// This way, the matrix is automatically transposed already, because XMLoadFloat4x4
		// will pick as row what is col and vice-versa. Therefore, ModelMatrix is Mesh->Transform
		// converted to row-major.
		memcpy(&Mesh->ModelMatrix, TransformColMajor, sizeof(XMFLOAT4X4));

		Mesh->PrimitivesCount = MeshData->primitives_count;
		Mesh->Primitives = S_ArenaAlloc(&Scene->SceneArena, Mesh->PrimitivesCount * sizeof(R_Primitive));

		for (cgltf_size PrimitiveId = 0; PrimitiveId < MeshData->primitives_count; PrimitiveId++) {
			cgltf_primitive *PrimitiveData = &MeshData->primitives[PrimitiveId];
			R_Primitive *Primitive = &Mesh->Primitives[PrimitiveId];
			cgltf_accessor *AccessorsData[cgltf_attribute_type_max_enum] = {0};
			cgltf_accessor *UVAccessorsData[2] = {0};

			for (int AttributeId = 0; AttributeId < PrimitiveData->attributes_count; ++AttributeId) {
				cgltf_attribute *AttributeData = &PrimitiveData->attributes[AttributeId];

				if (AttributeData->type == cgltf_attribute_type_texcoord) {
					if (AttributeData->index > 1) {
						continue;
					}
					UVAccessorsData[AttributeData->index] = AttributeData->vertex_data;
				} else {
					AccessorsData[AttributeData->type] = AttributeData->vertex_data;
				}
			}

			if (PrimitiveData->material) {
				cgltf_material *PrimitiveMaterialData = PrimitiveData->material;
				if (PrimitiveMaterialData->has_pbr_metallic_roughness) {
					cgltf_pbr_metallic_roughness *MetallicRoughnessData = &PrimitiveMaterialData->pbr_metallic_roughness;
					memcpy(&Primitive->cb.BaseColorFactor, MetallicRoughnessData->base_color_factor, sizeof(float) * 4);
					Primitive->cb.MetallicFactor = MetallicRoughnessData->metallic_factor;
					Primitive->cb.RoughnessFactor = MetallicRoughnessData->roughness_factor;
					
					if (MetallicRoughnessData->base_color_texture.texture) {
						Primitive->AlbedoIndex = MetallicRoughnessData->base_color_texture.texture->image - Data->images;

						if (MetallicRoughnessData->base_color_texture.has_transform) {
							cgltf_texture_transform *Transform = &MetallicRoughnessData->base_color_texture.transform;
							int uvIndex = MetallicRoughnessData->base_color_texture.texcoord;
							Primitive->UVChannel = uvIndex;
							Primitive->cb.UVScale.x = Transform->scale[0];
							Primitive->cb.UVScale.y = Transform->scale[1];
							Primitive->cb.UVOffset.x = Transform->offset[0];
							Primitive->cb.UVOffset.y = Transform->offset[1];
							Primitive->cb.UVRotation = Transform->rotation; 
						} else {
							Primitive->cb.UVScale.x = 1.0f;
							Primitive->cb.UVScale.y = 1.0f;
							Primitive->cb.UVOffset.x = 0.0f;
							Primitive->cb.UVOffset.y = 0.0f;
							Primitive->cb.UVRotation = 0.0f;
						}
					} else {
						Primitive->AlbedoIndex = -1;
					}

					if (MetallicRoughnessData->metallic_roughness_texture.texture) {
						Primitive->MetallicIndex = MetallicRoughnessData->metallic_roughness_texture.texture->image - Data->images;
					} else {
						Primitive->MetallicIndex = -1;
					}
				}

				cgltf_texture_view *NormalTextureView = &PrimitiveMaterialData->normal_texture;
				if (NormalTextureView->texture) {
					Primitive->NormalIndex = NormalTextureView->texture->image - Data->images;
				} else {
					Primitive->NormalIndex = -1;
				}

				if (PrimitiveMaterialData->pbr_metallic_roughness.metallic_roughness_texture.texture) {
					Primitive->RoughnessIndex = PrimitiveMaterialData->pbr_metallic_roughness.metallic_roughness_texture.texture->image - Data->images;
				} else {
					Primitive->RoughnessIndex = -1;
				}

				if (PrimitiveMaterialData->occlusion_texture.texture) {
					Primitive->OcclusionIndex = PrimitiveMaterialData->occlusion_texture.texture->image - Data->images;
				} else {
					Primitive->OcclusionIndex = -1;
				}
			}

			cgltf_accessor *PositionAccessor = AccessorsData[cgltf_attribute_type_position];
			if (!PositionAccessor) {
				S_LogAppend(L"Mesh has no POSITION attribute\n");
				break;
			}

			size_t VertexCount = PositionAccessor->count;
			R_Vertex *Vertices = S_ArenaAlloc(&Scene->SceneArena, sizeof(R_Vertex) * VertexCount);

			for (size_t i = 0; i < VertexCount; i++) {
				float Position[3];
				cgltf_accessor_read_float(PositionAccessor, i, Position, 3);
				Vertices[i].Position = (XMFLOAT3){Position[0], Position[1], Position[2]};

				cgltf_accessor *NormalAccessor = AccessorsData[cgltf_attribute_type_normal];
				if (NormalAccessor) {
					float Normal[4];
					cgltf_accessor_read_float(NormalAccessor, i, Normal, 3);
					Vertices[i].Normal.x = Normal[0];
					Vertices[i].Normal.y = Normal[1];
					Vertices[i].Normal.z = Normal[2];
				}

				if (UVAccessorsData[0]) {
					float uv[2];
					cgltf_accessor_read_float(UVAccessorsData[0], i, uv, 2);
					Vertices[i].UV0.x = uv[0];
					Vertices[i].UV0.y = uv[1];
				}

				if (UVAccessorsData[1]) {
					float uv[2];
					cgltf_accessor_read_float(UVAccessorsData[1], i, uv, 2);
					Vertices[i].UV1.x = uv[0];
					Vertices[i].UV1.y = uv[1];
				} else {
					Vertices[i].UV1.x = 0.0f;
					Vertices[i].UV1.y = 0.0f;
				}
			}

			cgltf_accessor *IndicesAccessor = PrimitiveData->indices;
			size_t IndexCount = IndicesAccessor ? IndicesAccessor->count : 0;

			uint16_t *Indices = NULL;
			if (IndexCount > 0) {
				Indices = S_ArenaAlloc(&Scene->SceneArena, sizeof(uint16_t) * IndexCount);
				for (size_t i = 0; i < IndexCount; i++) {
					uint32_t Index;
					cgltf_accessor_read_uint(IndicesAccessor, (int)i, &Index, 1);
					Indices[i] = (uint16_t)Index;
				}
			}

			Primitive->Vertices = Vertices;
			Primitive->VertexCount = VertexCount;
			Primitive->Indices = Indices;
			Primitive->IndexCount = IndexCount;
		}
		Scene->Models[Scene->ModelsCount].MeshesCount++;
	}

	cgltf_free(Data);
	S_ArenaRelease(&LocalArena);

	Scene->ModelsCount++;
	S_LogAppendf(L"Successfully loaded %s\n", Path);
	return TRUE;
}

/****************************************************
	Implementation of private functions
*****************************************************/

void
PreloadImages(S_Scene *Scene, cgltf_data *Data, PCWSTR Path)
{
	Scene->Models[Scene->ModelsCount].Images = S_ArenaAlloc(&Scene->SceneArena, Data->images_count * sizeof(R_Texture));
	Scene->Models[Scene->ModelsCount].ImagesCount = Data->images_count;

	for (int i = 0; i < Data->images_count; ++i) {
		cgltf_image *BaseImage = &Data->images[i];
		UINT8 *Pixels = NULL;
		size_t Size = 0;
		int W = 0, H = 0, Channels = 0;
		WCHAR BasePath[MAX_PATH];
		wcscpy_s(BasePath, MAX_PATH, Path);
		RemoveAllAfterLastSlash(BasePath);
		if (ExtractImageData(BasePath, BaseImage, &Pixels, &Size, &W, &H, &Channels)) {
			R_Texture *Texture = &Scene->Models[Scene->ModelsCount].Images[i];
			Texture->Pixels = Pixels;
			Texture->Width = W;
			Texture->Height = H;

			PWSTR UniqueNameW = S_ArenaAlloc(&Scene->SceneArena, MAX_PATH * sizeof(WCHAR));
			if (BaseImage->uri && !IsDataEmbedded(BaseImage)) {
				WCHAR UriW[MAX_PATH];
				MultiByteToWideChar(CP_UTF8, 0, BaseImage->uri, -1, UriW, MAX_PATH);
				swprintf_s(UniqueNameW, MAX_PATH, L"%s_%d_%s", Path, i, UriW);
			} else {
				swprintf_s(UniqueNameW, MAX_PATH, L"%s_Internal_%d", Path, i);
			}

			int UTF8Size = WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, NULL, 0, NULL, NULL);
			char *UniqueName = S_ArenaAlloc(&Scene->SceneArena, UTF8Size);
			WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, UniqueName, UTF8Size, NULL, NULL);
			Texture->Name = UniqueName;
		}
	}
}

BOOL
ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH],
				 _In_ cgltf_image *Img,
				 _Outptr_result_bytebuffer_(*Size) UINT8 **Pixels,
				 _Out_ size_t *Size,
				 _Out_ int *W,
				 _Out_ int *H,
				 _Out_ int *Channels)
{
	if (!Pixels || !Size || !W || !H || !Channels) {
		return FALSE;
	}

	*Pixels = NULL;
	*Size = 0;
	unsigned char *StbiData = NULL;

	if (Img->uri) {
		if (IsDataEmbedded(Img)) {
			const char *FirstCommaPtr = strchr(Img->uri, ',');
			if (!FirstCommaPtr) {
				return FALSE;
			}
			const char *EncodedB64 = FirstCommaPtr + 1;
			size_t EncLen = strlen(EncodedB64);
			size_t DecodedMaxCap = (EncLen / 4) * 3 + 4;
			char *Decoded = malloc(DecodedMaxCap);
			if (Decoded == NULL) {
				return FALSE;
			}
			b64_decode(EncodedB64, Decoded);
			StbiData = stbi_load_from_memory(Decoded, (int)DecodedMaxCap, W, H, Channels, 4);
			free(Decoded);
			if (!StbiData) {
				return FALSE;
			}
		} else {
			char FullPath[MAX_PATH];
			AppendFileNameToPath(BasePath, Img->uri, FullPath);
			StbiData = stbi_load(FullPath, W, H, Channels, 4);
			if (!StbiData) {
				return FALSE;
			}
		}
	} else if (Img->buffer_view) {
		cgltf_buffer_view *BufferView = Img->buffer_view;
		cgltf_buffer *Buffer = BufferView->buffer;
		const UINT8 *Data = (const UINT8 *)Buffer->vertex_data + BufferView->offset;
		StbiData = stbi_load_from_memory(Data, (int)BufferView->size, W, H, Channels, 4);
		if (StbiData == NULL) {
			return FALSE;
		}
	}

	if (StbiData == NULL) {
		return FALSE;
	}
	*Size = (size_t)(*W) * (size_t)(*H) * 4;
	*Pixels = malloc(*Size);
	if (*Pixels == NULL) {
		stbi_image_free(StbiData);
		return FALSE;
	}
	memcpy(*Pixels, StbiData, *Size);
	stbi_image_free(StbiData);
	return TRUE;
}

cgltf_result
LoadGLTFBuffer(
	_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _In_ S_Arena *Arena, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data)
{
	WCHAR FullPathBuffer[MAX_PATH];
	wcscpy_s(FullPathBuffer, MAX_PATH, FullPathGLTF);

	RemoveAllAfterLastSlash(FullPathBuffer);

	WCHAR BufferFileNameUTF8[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, BufferFileName, -1, BufferFileNameUTF8, MAX_PATH);
	wcscat_s(FullPathBuffer, MAX_PATH, BufferFileNameUTF8);

	FILE *file = _wfopen(FullPathBuffer, L"rb");
	if (!file) {
		return cgltf_result_file_not_found;
	}

	fseek(file, 0, SEEK_END);
	*Size = ftell(file);
	fseek(file, 0, SEEK_SET);

	*Data = S_ArenaAlloc(Arena, *Size);
	if (!*Data) {
		fclose(file);
		return cgltf_result_out_of_memory;
	}
	fread(*Data, 1, *Size, file);
	fclose(file);

	return cgltf_result_success;
}

void
RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH])
{
	PWSTR LastDoubleSlash = wcsrchr(FullPathBuffer, L'\\');
	PWSTR LastSlash = wcsrchr(FullPathBuffer, L'/');
	PWSTR Separator = (LastDoubleSlash > LastSlash) ? LastDoubleSlash : LastSlash;

	if (Separator) {
		*(Separator + 1) = L'\0'; // Null-terminate after the slash to keep the directory
	} else {
		FullPathBuffer[0] = L'\0'; // No slash found, file is in current working directory
	}
}

cgltf_result
LoadGLTFBuffers(_In_ const cgltf_options *Options, _In_ S_Arena *Arena, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path)
{
	if (Options == NULL) {
		return cgltf_result_invalid_options;
	}

	if (Data->buffers_count && Data->buffers[0].vertex_data == NULL && Data->buffers[0].uri == NULL && Data->bin) {
		if (Data->bin_size < Data->buffers[0].size) {
			return cgltf_result_data_too_short;
		}

		Data->buffers[0].vertex_data = (void *)Data->bin;
		Data->buffers[0].data_free_method = cgltf_data_free_method_none;
	}

	for (cgltf_size i = 0; i < Data->buffers_count; ++i) {
		if (Data->buffers[i].vertex_data) {
			continue;
		}

		const char *URI = Data->buffers[i].uri;

		if (URI == NULL) {
			continue;
		}

		if (strncmp(URI, "data:", 5) == 0) {
			char *Comma = strrchr(URI, ',');

			if (Comma && Comma - URI >= 7 && strncmp(Comma - 7, ";base64", 7) == 0) {
				cgltf_result Result = cgltf_load_buffer_base64(Options, Data->buffers[i].size, Comma + 1, &Data->buffers[i].vertex_data);
				Data->buffers[i].data_free_method = cgltf_data_free_method_memory_free;

				if (Result != cgltf_result_success) {
					return Result;
				}
			} else {
				return cgltf_result_unknown_format;
			}
		} else if (strstr(URI, "://") == NULL && Path) {
			cgltf_result Result = LoadGLTFBuffer(Path, URI, Arena, &Data->buffers[i].size, &Data->buffers[i].vertex_data);
			Data->buffers[i].data_free_method = cgltf_data_free_method_file_release;

			if (Result != cgltf_result_success) {
				return Result;
			}
		} else {
			return cgltf_result_unknown_format;
		}
	}

	return cgltf_result_success;
}

LONG
LoadGLTFFile(_In_z_ PCWSTR Path, S_Arena *Arena, _Outptr_ void **Data)
{
	FILE *file = _wfopen(Path, L"rb");
	if (!file) {
		return 0;
	}
	fseek(file, 0, SEEK_END);
	long Size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (Size <= 0) {
		fclose(file);
		return Size;
	}

	*Data = S_ArenaAlloc(Arena, Size);
	if (!*Data) {
		fclose(file);
		return 0;
	}

	size_t read_bytes = fread(*Data, 1, Size, file);
	fclose(file);

	if (read_bytes != (size_t)Size) {
		free(*Data);
		return 0;
	}

	return Size;
}

void
AppendFileNameToPath(_In_z_ PWSTR BasePathW, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH])
{
	char BasePart[MAX_PATH];
	WideCharToMultiByte(CP_UTF8, 0, BasePathW, -1, BasePart, MAX_PATH, NULL, NULL);
	strcpy_s(FullPath, MAX_PATH, BasePart);

	size_t len = strlen(FullPath);
	if (len > 0 && FullPath[len - 1] != '\\' && FullPath[len - 1] != '/') {
		strcat_s(FullPath, MAX_PATH, "\\");
	}

	strcat_s(FullPath, MAX_PATH, FileName);
}

int
IsDataEmbedded(const cgltf_image *const BaseImage)
{
	return BaseImage->uri && strncmp(BaseImage->uri, "data:", 5) == 0;
}

void *
cgltf_arena_alloc(void *user, cgltf_size size)
{
	return S_ArenaAlloc((S_Arena *)user, size);
}

void
cgltf_arena_free(void *user, void *ptr)
{
	// No-op: Arena handles lifetime
	(void)user;
	(void)ptr;
}