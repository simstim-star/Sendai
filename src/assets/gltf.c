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
#include <sal.h>

static const uint8_t WHITE_PIXEL[] = {255, 255, 255, 255};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void PreloadImages(SendaiScene *Scene, cgltf_data *Data, PCWSTR Path);

static BOOL ExtractImageData(
	_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_image *Img, _Outptr_result_bytebuffer_(*OutSize) uint8_t **Pixels, _Out_ size_t *Size, _Out_ int *W, _Out_ int *H,
	_Out_ int *Channels);

static void RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH]);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffers(_In_ const cgltf_options *Options, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path);

static cgltf_result LoadGLTFBuffer(_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data);

static void AppendFileNameToPath(_In_z_ PWSTR BasePath, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH]);

/****************************************************
	Public functions
*****************************************************/

BOOL SendaiGLTF_LoadModel(PCWSTR Path, SendaiScene *Scene)
{
	void *FileData = NULL;
	LONG Size = LoadGLTFFile(Path, &FileData);
	if (Size <= 0) {
		S_LogAppendf("Failed to load %s\n", Path);
		return FALSE;
	}

	cgltf_options Options = {0};
	cgltf_data *Data = NULL;
	cgltf_result Result = cgltf_parse(&Options, FileData, Size, &Data);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf("Failed to parse %s\n", Path);
		return FALSE;
	}

	Result = LoadGLTFBuffers(&Options, Data, Path);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf("Failed to load GLTF buffers from %s\n", Path);
		return FALSE;
	}

	if (Data->meshes_count == 0) {
		S_LogAppend("No meshes in glTF\n");
		cgltf_free(Data);
		return FALSE;
	}

	Scene->Models[Scene->ModelsCount].Meshes = S_ArenaAlloc(&Scene->SceneArena, Data->meshes_count * sizeof(R_Mesh));
	Scene->Models[Scene->ModelsCount].MeshesCount = Data->meshes_count;

	if (Data->images_count > 0) {
		PreloadImages(Scene, Data, Path);
	}

	for (size_t MeshId = 0; MeshId < Data->meshes_count; MeshId++) {
		cgltf_mesh *Mesh = &Data->meshes[MeshId];
		Scene->Models[Scene->ModelsCount].Meshes[MeshId].Primitives = S_ArenaAlloc(&Scene->SceneArena, Mesh->primitives_count * sizeof(R_Primitive));
		Scene->Models[Scene->ModelsCount].Meshes[MeshId].PrimitivesCount = Mesh->primitives_count;

		for (cgltf_size PrimitiveId = 0; PrimitiveId < Mesh->primitives_count; PrimitiveId++) {
			cgltf_primitive *Primitive = &Mesh->primitives[PrimitiveId];
			cgltf_accessor *PositionAccessor = NULL;
			cgltf_accessor *ColorAccessor = NULL;
			cgltf_accessor *UVAccessor = NULL;

			for (int AttributeId = 0; AttributeId < Primitive->attributes_count; ++AttributeId) {
				cgltf_attribute *Attribute = &Primitive->attributes[AttributeId];
				switch (Attribute->type) {
				case cgltf_attribute_type_position: {
					PositionAccessor = Attribute->vertex_data;
					break;
				}
				case cgltf_attribute_type_color:
					ColorAccessor = Attribute->vertex_data;
					break;
				case cgltf_attribute_type_texcoord:
					if (Attribute->index == 0) {
						UVAccessor = Attribute->vertex_data;
					}
					break;
				default:
					break;
				}
			}

			if (Primitive->material) {
				cgltf_pbr_metallic_roughness *PBR = &Primitive->material->pbr_metallic_roughness;
				if (PBR && PBR->base_color_texture.texture) {
					cgltf_image *TargetImage = PBR->base_color_texture.texture->image;
					Scene->Models[Scene->ModelsCount].Meshes[MeshId].BaseTextureIndex = TargetImage - Data->images;
				}
			}

			if (!PositionAccessor) {
				S_LogAppend("Mesh has no POSITION attribute\n");
				cgltf_free(Data);
				return FALSE;
			}

			size_t VertexCount = PositionAccessor->count;
			R_Vertex *Vertices = S_ArenaAlloc(&Scene->SceneArena, sizeof(R_Vertex) * VertexCount);

			for (size_t i = 0; i < VertexCount; i++) {
				float pos[3];
				cgltf_accessor_read_float(PositionAccessor, i, pos, 3);
				Vertices[i].Position = (R_Float4){pos[0], pos[1], -pos[2], 1.0f};

				if (ColorAccessor) {
					float c[4];
					cgltf_accessor_read_float(ColorAccessor, i, c, 4);
					Vertices[i].Color.X = c[0];
					Vertices[i].Color.Y = c[1];
					Vertices[i].Color.Z = c[2];
					Vertices[i].Color.W = 1.0f;
				} else {
					Vertices[i].Color = (R_Float4){1.0f, 1.0f, 1.0f, 1.0f};
				}

				if (UVAccessor) {
					float UV[2];
					cgltf_accessor_read_float(UVAccessor, i, UV, 2);
					Vertices[i].UV.U = UV[0];
					Vertices[i].UV.V = 1.0f - UV[1]; // DX Flip
				} else {
					Vertices[i].UV.U = 0.0f;
					Vertices[i].UV.V = 0.0f;
				}
			}

			cgltf_accessor *IndicesAccessor = Primitive->indices;
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

			Scene->Models[Scene->ModelsCount].Meshes[MeshId].Primitives[PrimitiveId].Vertices = Vertices;
			Scene->Models[Scene->ModelsCount].Meshes[MeshId].Primitives[PrimitiveId].VertexCount = VertexCount;
			Scene->Models[Scene->ModelsCount].Meshes[MeshId].Primitives[PrimitiveId].Indices = Indices;
			Scene->Models[Scene->ModelsCount].Meshes[MeshId].Primitives[PrimitiveId].IndexCount = IndexCount;
		}
	}

	cgltf_free(Data);

	Scene->ModelsCount++;
	S_LogAppendf(L"Successfully loaded %s\n", Path);
	return TRUE;
}

/****************************************************
	Implementation of private functions
*****************************************************/

void PreloadImages(SendaiScene *Scene, cgltf_data *Data, PCWSTR Path)
{
	Scene->Models[Scene->ModelsCount].Images = S_ArenaAlloc(&Scene->SceneArena, Data->images_count * sizeof(R_Texture));
	Scene->Models[Scene->ModelsCount].ImagesCount = Data->images_count;

	for (int i = 0; i < Data->images_count; ++i) {
		cgltf_image *BaseImage = &Data->images[i];
		uint8_t *Pixels = NULL;
		size_t Size = 0;
		int W = 0, H = 0, Channels = 0;
		WCHAR BasePath[MAX_PATH];
		wcscpy_s(BasePath, MAX_PATH, Path);
		RemoveAllAfterLastSlash(BasePath);
		if (ExtractImageData(BasePath, BaseImage, &Pixels, &Size, &W, &H, &Channels)) {
			Scene->Models[Scene->ModelsCount].Images[i].Pixels = Pixels;
			Scene->Models[Scene->ModelsCount].Images[i].Width = W;
			Scene->Models[Scene->ModelsCount].Images[i].Height = H;

			PWSTR UniqueNameW = S_ArenaAlloc(&Scene->SceneArena, MAX_PATH * sizeof(WCHAR));
			BOOL bIsDataEmbedded = strncmp(BaseImage->uri, "data:", 5) == 0;
			if (BaseImage->uri && !bIsDataEmbedded) {
				WCHAR UriW[MAX_PATH];
				MultiByteToWideChar(CP_UTF8, 0, BaseImage->uri, -1, UriW, MAX_PATH);
				swprintf_s(UniqueNameW, MAX_PATH, L"%s_%d_%s", Path, i, UriW);
			} else {
				swprintf_s(UniqueNameW, MAX_PATH, L"%s_Internal_%d", Path, i);
			}

			int UTF8Size = WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, NULL, 0, NULL, NULL);
			char *UniqueName = S_ArenaAlloc(&Scene->SceneArena, UTF8Size);
			WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, UniqueName, UTF8Size, NULL, NULL);
			Scene->Models[Scene->ModelsCount].Images[i].Name = UniqueName;
		}
	}
}

BOOL ExtractImageData(
	_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_image *Img, _Outptr_result_bytebuffer_(*OutSize) uint8_t **Pixels, _Out_ size_t *Size, _Out_ int *W, _Out_ int *H,
	_Out_ int *Channels)
{
	if (!Pixels || !Size || !W || !H || !Channels) {
		return FALSE;
	}

	*Pixels = NULL;
	*Size = 0;
	unsigned char *StbiData = NULL;

	if (Img->uri) {
		BOOL bIsDataEmbedded = strncmp(Img->uri, "data:", 5) == 0;
		if (bIsDataEmbedded) {
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
		const uint8_t *Data = (const uint8_t *)Buffer->vertex_data + BufferView->offset;
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

cgltf_result LoadGLTFBuffer(_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data)
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

	*Data = malloc(*Size);
	if (!*Data) {
		fclose(file);
		return cgltf_result_out_of_memory;
	}

	fread(*Data, 1, *Size, file);
	fclose(file);

	return cgltf_result_success;
}

void RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH])
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

cgltf_result LoadGLTFBuffers(_In_ const cgltf_options *Options, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path)
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
			cgltf_result Result = LoadGLTFBuffer(Path, URI, &Data->buffers[i].size, &Data->buffers[i].vertex_data);
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

LONG LoadGLTFFile(_In_z_ PCWSTR Path, _Outptr_ void **Data)
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

	*Data = malloc(Size);
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

void AppendFileNameToPath(_In_z_ PWSTR BasePathW, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH])
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