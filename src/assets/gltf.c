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

static int LoadGLTFImage(
	_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_data *Data, _In_ cgltf_image *Img, _Outptr_result_bytebuffer_(*OutSize) uint8_t **Pixels, _Out_ size_t *Size,
	_Out_ int *W, _Out_ int *H, _Out_ int *Channels);

static void RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH]);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffers(_In_ const cgltf_options *options, _Inout_ cgltf_data *vertex_data, _In_z_ PCWSTR gltf_path);

static cgltf_result LoadGLTFBuffer(_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data);

static void AppendFileNameToPath(_In_z_ WCHAR *BasePath, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH]);

/****************************************************
	Public functions
*****************************************************/

BOOL SendaiGLTF_load(PCWSTR Path, SendaiScene *OutScene)
{
	void *FileData = NULL;
	LONG Size = LoadGLTFFile(Path, &FileData);
	if (Size <= 0) {
		return false;
	}

	cgltf_options Options = {0};
	cgltf_data *Data = NULL;
	cgltf_result Result = cgltf_parse(&Options, FileData, Size, &Data);

	if (Result == cgltf_result_success) {
		Result = LoadGLTFBuffers(&Options, Data, Path);
	}

	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		return FALSE;
	}

	OutScene->Meshes = SendaiArena_alloc(&OutScene->SceneArena, Data->meshes_count * sizeof(R_Mesh));
	OutScene->MeshCount = Data->meshes_count;
	if (Data->meshes_count == 0) {
		S_LogAppend("No meshes in glTF\n");
		cgltf_free(Data);
		return false;
	}
	size_t ImagesCount = Data->images_count || 1;

	R_Texture *Textures = SendaiArena_alloc(&OutScene->SceneArena, Data->meshes_count * ImagesCount * sizeof(R_Texture));
	for (size_t MeshId = 0; MeshId < Data->meshes_count; MeshId++) {
		OutScene->Meshes[MeshId].Textures = &Textures[MeshId];
		OutScene->Meshes[MeshId].TextureCount = ImagesCount;

		for (size_t i = 0; i < ImagesCount; ++i) {
			uint8_t *Pixels = NULL;
			size_t Size = 0;
			int W = 0, H = 0, Channels = 0;

			int Loaded = 0;
			if (i < Data->images_count) {
				WCHAR BasePath[MAX_PATH];
				wcscpy_s(BasePath, MAX_PATH, Path);
				RemoveAllAfterLastSlash(BasePath);
				Loaded = LoadGLTFImage(BasePath, Data, &Data->images[i], &Pixels, &Size, &W, &H, &Channels);
			}

			if (!Loaded) {
				OutScene->Meshes[MeshId].Textures[i].Pixels = &WHITE_PIXEL;
				OutScene->Meshes[MeshId].Textures[i].Width = 1;
				OutScene->Meshes[MeshId].Textures[i].Height = 1;
			} else {
				OutScene->Meshes[MeshId].Textures[i].Pixels = Pixels;
				OutScene->Meshes[MeshId].Textures[i].Width = W;
				OutScene->Meshes[MeshId].Textures[i].Height = H;
			}
		}

		cgltf_mesh *Mesh = &Data->meshes[0];
		cgltf_primitive *Primitive = &Mesh->primitives[0];

		cgltf_accessor *PositionAccessor = NULL;
		cgltf_accessor *ColorAccessor = NULL;
		cgltf_accessor *UVAccessor = NULL;

		for (cgltf_size i = 0; i < Primitive->attributes_count; i++) {
			cgltf_attribute *Attribute = &Primitive->attributes[i];
			switch (Attribute->type) {
			case cgltf_attribute_type_position:
				PositionAccessor = Attribute->vertex_data;
				break;
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

		if (!PositionAccessor) {
			S_LogAppend("Mesh has no POSITION attribute\n");
			cgltf_free(Data);
			return false;
		}

		size_t VertexCount = PositionAccessor->count;
		R_Vertex *Vertices = SendaiArena_alloc(&OutScene->SceneArena, sizeof(R_Vertex) * VertexCount);

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
			Indices = SendaiArena_alloc(&OutScene->SceneArena, sizeof(uint16_t) * IndexCount);
			for (size_t i = 0; i < IndexCount; i++) {
				uint32_t Index;
				cgltf_accessor_read_uint(IndicesAccessor, (int)i, &Index, 1);
				Indices[i] = (uint16_t)Index;
			}
		}

		OutScene->Meshes[MeshId].Vertices = Vertices;
		OutScene->Meshes[MeshId].VertexCount = VertexCount;
		OutScene->Meshes[MeshId].Indices = Indices;
		OutScene->Meshes[MeshId].IndexCount = IndexCount;
	}

	cgltf_free(Data);

	S_LogAppendf(L"Successfully loaded %s\n", Path);
	return true;
}

/****************************************************
	Implementation of private functions
*****************************************************/

int LoadGLTFImage(
	_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_data *Data, _In_ cgltf_image *Img, _Outptr_result_bytebuffer_(*OutSize) uint8_t **Pixels, _Out_ size_t *Size,
	_Out_ int *W, _Out_ int *H, _Out_ int *Channels)
{
	if (!Pixels || !Size || !W || !H || !Channels) {
		return 0;
	}

	*Pixels = NULL;
	*Size = 0;
	unsigned char *StbiData = NULL;

	if (Img->uri) {
		BOOL bDataEmbedded = strncmp(Img->uri, "data:", 5) == 0;
		if (bDataEmbedded) {
			const char *FirstCommaPtr = strchr(Img->uri, ',');
			if (!FirstCommaPtr) {
				return 0;
			}
			const char *EncodedB64 = FirstCommaPtr + 1;
			size_t DecodedLen = strlen(EncodedB64) * 3 / 4;
			char *Decoded = malloc(DecodedLen);
			b64_decode(EncodedB64, Decoded);
			StbiData = stbi_load_from_memory(Decoded, (int)DecodedLen, W, H, Channels, 4);
			free(Decoded);
			if (!StbiData) {
				return 0;
			}
		} else {
			char FullPath[MAX_PATH];
			AppendFileNameToPath(BasePath, Img->uri, FullPath);
			StbiData = stbi_load(FullPath, W, H, Channels, 4);
			if (!StbiData) {
				return 0;
			}
		}
	} else if (Img->buffer_view) {
		cgltf_buffer_view *BufferView = Img->buffer_view;
		cgltf_buffer *Buffer = BufferView->buffer;
		const uint8_t *Data = (const uint8_t *)Buffer->vertex_data + BufferView->offset;
		StbiData = stbi_load_from_memory(Data, (int)BufferView->size, W, H, Channels, 4);
		if (StbiData == NULL) {
			return 0;
		}
	}

	if (StbiData == NULL) {
		return 1;
	}
	*Size = (size_t)(*W) * (size_t)(*H) * 4;
	*Pixels = malloc(*Size);
	if (*Pixels == NULL) {
		stbi_image_free(StbiData);
		return 1;
	}
	memcpy(*Pixels, StbiData, *Size);
	stbi_image_free(StbiData);
	return 1;
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
	WCHAR *LastDoubleSlash = wcsrchr(FullPathBuffer, L'\\');
	WCHAR *LastSlash = wcsrchr(FullPathBuffer, L'/');
	WCHAR *Separator = (LastDoubleSlash > LastSlash) ? LastDoubleSlash : LastSlash;

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

		if (wcsncmp(URI, L"data:", 5) == 0) {
			const WCHAR *Comma = wcsrchr(URI, L',');

			if (Comma && Comma - URI >= 7 && wcsncmp(Comma - 7, L";base64", 7) == 0) {
				cgltf_result Result = cgltf_load_buffer_base64(Options, Data->buffers[i].size, Comma + 1, &Data->buffers[i].vertex_data);
				Data->buffers[i].data_free_method = cgltf_data_free_method_memory_free;

				if (Result != cgltf_result_success) {
					return Result;
				}
			} else {
				return cgltf_result_unknown_format;
			}
		} else if (wcsstr(URI, L"://") == NULL && Path) {
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

void AppendFileNameToPath(_In_z_ WCHAR *BasePathW, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH])
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