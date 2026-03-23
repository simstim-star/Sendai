#include "../core/pch.h"

#define CGLTF_IMPLEMENTATION
#include "../../deps/cgltf.h"

#include "../core/memory.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "../../deps/stb_image.h"

#include "../../deps/b64.h"

#include "../core/log.h"
#include "../core/scene.h"
#include "../win32/win_path.h"
#include "../renderer/render_types.h"
#include "gltf.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void PreloadImages(R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena *UploadArena);

static BOOL ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH],
							 _In_ cgltf_image *Img,
							 _In_ M_Arena *SceneArena,
							 _Outptr_result_bytebuffer_(*Size) UINT8 **Pixels,
							 _Out_ size_t *Size,
							 _Out_ int *W,
							 _Out_ int *H,
							 _Out_ int *Channels);

static void RemoveAllAfterLastSlash(_Inout_updates_z_(MAX_PATH) WCHAR FullPathBuffer[MAX_PATH]);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _In_ M_Arena *Arena, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffers(_In_ const cgltf_options *Options, _In_ M_Arena *Arena, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path);

static cgltf_result LoadGLTFBuffer(
	_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _In_ M_Arena *Arena, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data);

static void AppendFileNameToPath(_In_z_ PWSTR BasePath, _In_z_ char *FileName, _Out_writes_z_(MAX_PATH) char FullPath[MAX_PATH]);

static PSTR CreateTextureName(M_Arena *UploadArena, cgltf_image *BaseImage, PCWSTR Path, int i);

static int IsDataEmbedded(const cgltf_image *const BaseImage);

static void RetriveAttributeData(cgltf_primitive *PrimitiveData, cgltf_accessor *UVAccessorsData[2], cgltf_accessor *AccessorsData[9]);

static void LoadPBRData(R_Model *Model, R_Primitive *Primitive, cgltf_primitive *PrimitiveData, cgltf_data *Data);

static void LoadVertexData(
	cgltf_accessor *AccessorsData[9], S_Scene *Scene, cgltf_accessor *UVAccessorsData[2], cgltf_primitive *PrimitiveData, R_Primitive *Primitive);

// The below functions are to inject into gltf to use my arena
static void *cgltf_arena_alloc(void *user, cgltf_size size);
static void cgltf_arena_free(void *user, void *ptr);

/****************************************************
	Public functions
*****************************************************/

BOOL
SendaiGLTF_LoadModel(PCWSTR Path, S_Scene *Scene)
{
	void *FileData = NULL;

	LONG Size = LoadGLTFFile(Path, &Scene->UploadArena, &FileData);
	if (Size <= 0) {
		S_LogAppendf(L"Failed to load %s\n", Path);
		return FALSE;
	}

	cgltf_options Options = {0};
	Options.memory.alloc_func = cgltf_arena_alloc;
	Options.memory.free_func = cgltf_arena_free;
	Options.memory.user_data = &Scene->UploadArena;

	cgltf_data *Data = NULL;
	cgltf_result Result = cgltf_parse(&Options, FileData, Size, &Data);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to parse %s\n", Path);
		return FALSE;
	}

	Result = LoadGLTFBuffers(&Options, &Scene->UploadArena, Data, Path);
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

	R_Model *Model = &Scene->Models[Scene->ModelsCount];
	Model->Meshes = M_ArenaAlloc(&Scene->SceneArena, Data->nodes_count * sizeof(R_Mesh));
	Model->MeshesCount = Data->nodes_count;
	WCHAR FileNameW[MAX_PATH];
	Win32GetFileNameOnly(Path, FileNameW, MAX_PATH);
	int UTF8Size = WideCharToMultiByte(CP_UTF8, 0, FileNameW, -1, NULL, 0, NULL, NULL);
	Model->Name = M_ArenaAlloc(&Scene->SceneArena, UTF8Size);
	WideCharToMultiByte(CP_UTF8, 0, FileNameW, -1, Model->Name, UTF8Size, NULL, NULL);

	if (Data->images_count > 0) {
		PreloadImages(Model, Data, Path, &Scene->UploadArena);
	}

	size_t NodeCount = Data->nodes_count;
	Model->MeshesCount = 0;
	Model->Scale.x = 1;
	Model->Scale.y = 1;
	Model->Scale.z = 1;
	Model->Rotation.x = 0.0f;
	Model->Rotation.y = 0.0f;
	Model->Rotation.z = 0.0f;

	// I'm simplyfing Node == Mesh here, as my renderer is single Camera
	for (size_t NodeIndex = 0; NodeIndex < NodeCount; NodeIndex++, Model->MeshesCount++) {
		cgltf_node *NodeData = &Data->nodes[NodeIndex];
		if (NodeData->mesh == NULL) {
			continue;
		}

		UINT CurrentMeshIndex = Model->MeshesCount;
		R_Mesh *CurrentMesh = &Model->Meshes[CurrentMeshIndex];
		cgltf_mesh *NodeMeshData = NodeData->mesh;

		cgltf_float TransformColMajor[4][4];
		cgltf_node_transform_world(NodeData, TransformColMajor);

		// Note: Mesh->Transform is col-major, but XMLoadFloat4x4 expects row-major.
		// This way, the matrix is automatically transposed already, because XMLoadFloat4x4
		// will pick as row what is col and vice-versa. Therefore, ModelMatrix is Mesh->Transform
		// converted to row-major.
		memcpy(&CurrentMesh->ModelMatrix, TransformColMajor, sizeof(XMFLOAT4X4));

		CurrentMesh->PrimitivesCount = NodeMeshData->primitives_count;
		CurrentMesh->Primitives = M_ArenaAlloc(&Scene->SceneArena, CurrentMesh->PrimitivesCount * sizeof(R_Primitive));

		for (cgltf_size PrimitiveId = 0; PrimitiveId < NodeMeshData->primitives_count; PrimitiveId++) {
			cgltf_primitive *PrimitiveData = &NodeMeshData->primitives[PrimitiveId];
			
			cgltf_accessor *AccessorsData[cgltf_attribute_type_max_enum] = {0};
			cgltf_accessor *UVAccessorsData[2] = {0};
			RetriveAttributeData(PrimitiveData, UVAccessorsData, AccessorsData);

			R_Primitive *Primitive = &CurrentMesh->Primitives[PrimitiveId];
			LoadPBRData(Model, Primitive, PrimitiveData, Data);
			LoadVertexData(AccessorsData, Scene, UVAccessorsData, PrimitiveData, Primitive);
			
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

void
RetriveAttributeData(cgltf_primitive *PrimitiveData, cgltf_accessor *UVAccessorsData[2], cgltf_accessor *AccessorsData[9])
{
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
}

void
LoadVertexData(
	cgltf_accessor *AccessorsData[9], S_Scene *Scene, cgltf_accessor *UVAccessorsData[2], cgltf_primitive *PrimitiveData, R_Primitive *Primitive)
{
	cgltf_accessor *PositionAccessor = AccessorsData[cgltf_attribute_type_position];
	if (!PositionAccessor) {
		S_LogAppend(L"Mesh has no POSITION attribute\n");
		{
			return;
		};
	}

	UINT VertexCount = PositionAccessor->count;
	R_Vertex *Vertices = M_ArenaAlloc(&Scene->UploadArena, sizeof(R_Vertex) * VertexCount);

	for (UINT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++) {
		float Position[3];
		cgltf_accessor_read_float(PositionAccessor, VertexIndex, Position, 3);
		Vertices[VertexIndex].Position = (XMFLOAT3){Position[0], Position[1], Position[2]};

		cgltf_accessor *NormalAccessor = AccessorsData[cgltf_attribute_type_normal];
		if (NormalAccessor) {
			float Normal[4];
			cgltf_accessor_read_float(NormalAccessor, VertexIndex, Normal, 3);
			Vertices[VertexIndex].Normal.x = Normal[0];
			Vertices[VertexIndex].Normal.y = Normal[1];
			Vertices[VertexIndex].Normal.z = Normal[2];
		}

		if (UVAccessorsData[0]) {
			float uv[2];
			cgltf_accessor_read_float(UVAccessorsData[0], VertexIndex, uv, 2);
			Vertices[VertexIndex].UV0.x = uv[0];
			Vertices[VertexIndex].UV0.y = uv[1];
		}

		if (UVAccessorsData[1]) {
			float uv[2];
			cgltf_accessor_read_float(UVAccessorsData[1], VertexIndex, uv, 2);
			Vertices[VertexIndex].UV1.x = uv[0];
			Vertices[VertexIndex].UV1.y = uv[1];
		} else {
			Vertices[VertexIndex].UV1.x = 0.0f;
			Vertices[VertexIndex].UV1.y = 0.0f;
		}
	}

	cgltf_accessor *IndicesAccessor = PrimitiveData->indices;
	size_t IndexCount = IndicesAccessor ? IndicesAccessor->count : 0;

	uint16_t *Indices = NULL;
	if (IndexCount > 0) {
		Indices = M_ArenaAlloc(&Scene->UploadArena, sizeof(uint16_t) * IndexCount);
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

void
PreloadImages(R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena* UploadArena)
{
	Model->Images = M_ArenaAlloc(UploadArena, Data->images_count * sizeof(R_Texture));
	Model->ImagesCount = Data->images_count;

	for (int i = 0; i < Data->images_count; ++i) {
		cgltf_image *BaseImage = &Data->images[i];
		UINT8 *Pixels = NULL;
		size_t Size = 0;
		int W = 0, H = 0, Channels = 0;
		WCHAR BasePath[MAX_PATH];
		wcscpy_s(BasePath, MAX_PATH, Path);
		RemoveAllAfterLastSlash(BasePath);
		if (ExtractImageData(BasePath, BaseImage, UploadArena, &Pixels, &Size, &W, &H, &Channels)) {
			Model->Images[i].Pixels = Pixels;
			Model->Images[i].Width = W;
			Model->Images[i].Height = H;
			Model->Images[i].Name = CreateTextureName(UploadArena, BaseImage, Path, i);
		}
	}
}

BOOL
ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH],
				 _In_ cgltf_image *Img,
				 _In_ M_Arena *UploadArena,
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
			stbi_uc *Decoded = M_ArenaAlloc(UploadArena, DecodedMaxCap);
			if (Decoded == NULL) {
				return FALSE;
			}
			b64_decode(EncodedB64, Decoded);
			StbiData = stbi_load_from_memory(Decoded, (int)DecodedMaxCap, W, H, Channels, 4);
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
	*Pixels = M_ArenaAlloc(UploadArena, *Size);
	memcpy(*Pixels, StbiData, *Size);
	stbi_image_free(StbiData);
	return TRUE;
}

cgltf_result
LoadGLTFBuffer(
	_In_z_ PCWSTR FullPathGLTF, _In_z_ PCWSTR BufferFileName, _In_ M_Arena *Arena, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data)
{
	WCHAR FullPathBuffer[MAX_PATH];
	wcscpy_s(FullPathBuffer, MAX_PATH, FullPathGLTF);
	RemoveAllAfterLastSlash(FullPathBuffer);
	wcscat_s(FullPathBuffer, MAX_PATH, BufferFileName);

	FILE *file = _wfopen(FullPathBuffer, L"rb");
	if (!file) {
		return cgltf_result_file_not_found;
	}

	fseek(file, 0, SEEK_END);
	*Size = ftell(file);
	fseek(file, 0, SEEK_SET);

	*Data = M_ArenaAlloc(Arena, *Size);
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
LoadGLTFBuffers(_In_ const cgltf_options *Options, _In_ M_Arena *Arena, _Inout_ cgltf_data *Data, _In_z_ PCWSTR Path)
{
	if (Options == NULL) {
		return cgltf_result_invalid_options;
	}

	if (Data->buffers_count && Data->buffers[0].vertex_data == NULL && Data->buffers[0].uri == NULL && Data->bin) {
		if (Data->bin_size < Data->buffers[0].size) {
			return cgltf_result_data_too_short;
		}
		Data->buffers[0].vertex_data = Data->bin;
		Data->buffers[0].data_free_method = cgltf_data_free_method_none;
	}

	for (cgltf_size i = 0; i < Data->buffers_count; ++i) {
		if (Data->buffers[i].vertex_data) {
			continue;
		}

		const char *Uri = Data->buffers[i].uri;

		if (Uri == NULL) {
			continue;
		}

		if (strncmp(Uri, "data:", 5) == 0) {
			char *Comma = strrchr(Uri, ',');

			if (Comma && Comma - Uri >= 7 && strncmp(Comma - 7, ";base64", 7) == 0) {
				cgltf_result Result = cgltf_load_buffer_base64(Options, Data->buffers[i].size, Comma + 1, &Data->buffers[i].vertex_data);
				Data->buffers[i].data_free_method = cgltf_data_free_method_memory_free;

				if (Result != cgltf_result_success) {
					return Result;
				}
			} else {
				return cgltf_result_unknown_format;
			}
		} else if (strstr(Uri, "://") == NULL && Path) {
			WCHAR UriW[MAX_PATH];
			MultiByteToWideChar(CP_UTF8, 0, Uri, -1, UriW, MAX_PATH);
			cgltf_result Result = LoadGLTFBuffer(Path, UriW, Arena, &Data->buffers[i].size, &Data->buffers[i].vertex_data);
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
LoadGLTFFile(_In_z_ PCWSTR Path, M_Arena *Arena, _Outptr_ void **Data)
{
	FILE *FileHandle = _wfopen(Path, L"rb");
	if (!FileHandle) {
		return 0;
	}
	fseek(FileHandle, 0, SEEK_END);
	long Size = ftell(FileHandle);
	fseek(FileHandle, 0, SEEK_SET);

	if (Size <= 0) {
		fclose(FileHandle);
		return Size;
	}

	*Data = M_ArenaAlloc(Arena, Size);
	if (!*Data) {
		fclose(FileHandle);
		return 0;
	}

	size_t ReadBytes = fread(*Data, 1, Size, FileHandle);
	fclose(FileHandle);

	if (ReadBytes != (size_t)Size) {
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

PSTR
CreateTextureName(M_Arena *UploadArena, cgltf_image *BaseImage, PCWSTR Path, int i)
{
	PWSTR UniqueNameW = M_ArenaAlloc(UploadArena, MAX_PATH * sizeof(WCHAR));
	if (BaseImage->uri && !IsDataEmbedded(BaseImage)) {
		WCHAR UriW[MAX_PATH];
		MultiByteToWideChar(CP_UTF8, 0, BaseImage->uri, -1, UriW, MAX_PATH);
		swprintf_s(UniqueNameW, MAX_PATH, L"%s_%d_%s", Path, i, UriW);
	} else {
		swprintf_s(UniqueNameW, MAX_PATH, L"%s_Internal_%d", Path, i);
	}

	int UTF8Size = WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, NULL, 0, NULL, NULL);
	PSTR UniqueName = M_ArenaAlloc(UploadArena, UTF8Size);
	WideCharToMultiByte(CP_UTF8, 0, UniqueNameW, -1, UniqueName, UTF8Size, NULL, NULL);
	return UniqueName;
}

void
LoadPBRData(R_Model *Model, R_Primitive *Primitive, cgltf_primitive *PrimitiveData, cgltf_data *Data)
{
	const R_Texture *const Images = Model->Images;
	Primitive->Albedo = NULL;
	Primitive->Normal = NULL;
	Primitive->Metallic = NULL;
	Primitive->Roughness = NULL;
	Primitive->Occlusion = NULL;
	Primitive->Emissive = NULL;
	if (PrimitiveData->material) {
		cgltf_material *PrimitiveMaterialData = PrimitiveData->material;
		if (PrimitiveMaterialData->has_pbr_metallic_roughness) {
			cgltf_pbr_metallic_roughness *MetallicRoughnessData = &PrimitiveMaterialData->pbr_metallic_roughness;
			memcpy(&Primitive->cb.BaseColorFactor, MetallicRoughnessData->base_color_factor, sizeof(float) * 4);
			Primitive->cb.MetallicFactor = MetallicRoughnessData->metallic_factor;
			Primitive->cb.RoughnessFactor = MetallicRoughnessData->roughness_factor;

			if (MetallicRoughnessData->base_color_texture.texture) {
				UINT AlbedoIndex = MetallicRoughnessData->base_color_texture.texture->image - Data->images;
				Primitive->Albedo = &Images[AlbedoIndex];

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
			}

			if (MetallicRoughnessData->metallic_roughness_texture.texture) {
				UINT MetallicIndex = MetallicRoughnessData->metallic_roughness_texture.texture->image - Data->images;
				Primitive->Metallic = &Images[MetallicIndex];
			}
		}

		cgltf_texture_view *NormalTextureView = &PrimitiveMaterialData->normal_texture;
		if (NormalTextureView->texture) {
			Primitive->Normal = &Images[NormalTextureView->texture->image - Data->images];
		}

		if (PrimitiveMaterialData->pbr_metallic_roughness.metallic_roughness_texture.texture) {
			Primitive->Roughness = &Images[PrimitiveMaterialData->pbr_metallic_roughness.metallic_roughness_texture.texture->image - Data->images];
		}

		if (PrimitiveMaterialData->occlusion_texture.texture) {
			Primitive->Occlusion = &Images[PrimitiveMaterialData->occlusion_texture.texture->image - Data->images];
		}

		memcpy(&Primitive->cb.EmissiveFactor, PrimitiveMaterialData->emissive_factor, sizeof(cgltf_float) * 3);
		if (PrimitiveMaterialData->emissive_texture.texture) {
			Primitive->Emissive = &Images[PrimitiveMaterialData->emissive_texture.texture->image - Data->images];
		}
	}
}

void *
cgltf_arena_alloc(void *user, cgltf_size size)
{
	return M_ArenaAlloc((M_Arena *)user, size);
}

void
cgltf_arena_free(void *user, void *ptr)
{
	// No-op: Arena handles lifetime
	(void)user;
	(void)ptr;
}