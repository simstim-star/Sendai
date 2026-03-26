#include "../core/pch.h"

#include "gltf.h"

#define CGLTF_IMPLEMENTATION
#include "../../deps/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "../../deps/stb_image.h"

#include "../../deps/stb_ds.h"

#include "../../deps/b64.h"

#include "../core/log.h"
#include "../core/memory.h"
#include "../core/scene.h"
#include "../renderer/render_types.h"
#include "../renderer/renderer.h"
#include "../renderer/texture.h"
#include "../win32/win_path.h"

#define UTF8_SIZE(StrW) WideCharToMultiByte(CP_UTF8, 0, StrW, -1, NULL, 0, NULL, NULL)
#define W_TO_UTF8(StrW, StrUTF8, UTF8Size) WideCharToMultiByte(CP_UTF8, 0, StrW, -1, StrUTF8, UTF8Size, NULL, NULL)
#define UTF8_TO_W(Str, StrW, WSize) MultiByteToWideChar(CP_UTF8, 0, Str, -1, StrW, WSize)

typedef struct MeshLookup {
	char *key;
	R_Mesh *Mesh;
} MeshLookup;

/****************************************************
	Forward declaration of private functions
*****************************************************/

static cgltf_data *GetData(PCWSTR Path, M_Arena *UploadArena);

static void SetModelName(PCWSTR Path, R_Model *Model, M_Arena *Arena);

static void LoadNodes(R_Core *Renderer, R_Model *Model, cgltf_data *Data, M_Arena *SceneArena, M_Arena *UploadArena);

static void PreloadImages(R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena *UploadArena);

static BOOL ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_image *Img, _In_ M_Arena *UploadArena, _Out_ R_Texture *Texture);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _In_ M_Arena *Arena, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffer(_In_z_ PCWSTR FullPath,
								   _In_z_ PCWSTR BufferFileName,
								   _In_ M_Arena *Arena,
								   _Out_ size_t *Size,
								   _Outptr_result_bytebuffer_(*Size) void **Data);

static PSTR CreateTextureName(M_Arena *UploadArena, cgltf_image *BaseImage, PCWSTR Path, int i);

static BOOL IsDataEmbedded(const cgltf_image *const BaseImage);

static void RetriveAttributeData(cgltf_primitive *PrimitiveData, cgltf_accessor *UVAccessorsData[2], cgltf_accessor *AccessorsData[9]);

static void
LoadPBRData(R_Core *Renderer, const R_Texture *const Images, cgltf_image *ImagesData, cgltf_material *Material, R_PBRConstantBuffer *CB);

static void LoadVerticesAndIndicesIntoBuffers(R_Core *Renderer,
											  R_Primitive *Primitive,
											  cgltf_accessor *PositionAccessor,
											  cgltf_accessor *NormalAccessor,
											  cgltf_accessor *IndicesAccessor,
											  cgltf_accessor *UVAccessorsData[2],
											  M_Arena *Arena);

/* The below functions are to inject into gltf loader to use my arena */

static void *
cgltf_arena_alloc(void *user, cgltf_size size)
{
	return M_ArenaAlloc((M_Arena *)user, size);
}

static void
cgltf_arena_free(void *user, void *ptr)
{
	/* arena handles lifetime */
}

/****************************************************
	Public functions
*****************************************************/

BOOL
SendaiGLTF_LoadModel(R_Core *Renderer, PCWSTR Path, S_Scene *Scene)
{
	cgltf_data *Data = GetData(Path, &Scene->UploadArena);
	if (Data == NULL) {
		return FALSE;
	}

	R_Model *Model = &Scene->Models[Scene->ModelsCount];
	Model->Scale = (XMFLOAT3){.x = 1, .y = 1, .z = 1};
	Model->Visible = TRUE;
	SetModelName(Path, Model, &Scene->SceneArena);

	if (Data->images_count > 0) {
		PreloadImages(Model, Data, Path, &Scene->UploadArena);
	}

	LoadNodes(Renderer, Model, Data, &Scene->SceneArena, &Scene->UploadArena);

	cgltf_free(Data);

	D3D12_RESOURCE_BARRIER VBBarrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
										.Transition = {.pResource = Renderer->VertexBufferDefault,
													   .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
													   .StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
													   .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES}};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &VBBarrier);

	D3D12_RESOURCE_BARRIER IBBarrier = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
										.Transition = {.pResource = Renderer->IndexBufferDefault,
													   .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
													   .StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER,
													   .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES}};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &IBBarrier);

	Scene->ModelsCount++;
	S_LogAppendf(L"Successfully loaded %s\n", Path);
	return TRUE;
}

/****************************************************
	Implementation of private functions
*****************************************************/

void
LoadNodes(R_Core *Renderer, R_Model *Model, cgltf_data *Data, M_Arena *SceneArena, M_Arena *UploadArena)
{
	R_Mesh *Meshes = M_ArenaAlloc(SceneArena, Data->meshes_count * sizeof(R_Mesh));

	MeshLookup *MeshMap = NULL;

	for (INT MeshIndex = 0; MeshIndex < Data->meshes_count; MeshIndex++) {
		cgltf_mesh *MeshData = &Data->meshes[MeshIndex];
		R_Mesh *CurrentMesh = &Meshes[MeshIndex];

		CurrentMesh->PrimitivesCount = MeshData->primitives_count;
		CurrentMesh->Primitives = M_ArenaAlloc(SceneArena, CurrentMesh->PrimitivesCount * sizeof(R_Primitive));

		for (cgltf_size PrimitiveId = 0; PrimitiveId < MeshData->primitives_count; PrimitiveId++) {
			cgltf_primitive *PrimitiveData = &MeshData->primitives[PrimitiveId];

			cgltf_accessor *Accessors[cgltf_attribute_type_max_enum] = {0};
			cgltf_accessor *UVAccessorsData[2] = {0};
			RetriveAttributeData(PrimitiveData, UVAccessorsData, Accessors);

			R_Primitive *Primitive = &CurrentMesh->Primitives[PrimitiveId];
			LoadPBRData(Renderer, Model->Images, Data->images, PrimitiveData->material, &Primitive->ConstantBuffer);

			LoadVerticesAndIndicesIntoBuffers(Renderer, Primitive, Accessors[cgltf_attribute_type_position],
											  Accessors[cgltf_attribute_type_normal], PrimitiveData->indices, UVAccessorsData, UploadArena);
		}

		MeshLookup Lookup = {.key = MeshData->name, .Mesh = CurrentMesh};
		shputs(MeshMap, Lookup);
	}

	Model->Nodes = M_ArenaAlloc(SceneArena, Data->nodes_count * sizeof(R_Node));
	Model->NodesCount = 0;

	for (; Model->NodesCount < Data->nodes_count; Model->NodesCount++) {
		UINT NodeIndex = Model->NodesCount;
		cgltf_node *NodeData = &Data->nodes[NodeIndex];
		R_Node *CurrentNode = &Model->Nodes[NodeIndex];

		cgltf_float TransformColMajor[4][4];
		cgltf_node_transform_world(NodeData, TransformColMajor);
		// Note: Mesh->Transform is col-major, but XMLoadFloat4x4 expects row-major.
		// This way, the matrix is automatically transposed already, because XMLoadFloat4x4
		// will pick as row what is col and vice-versa. Therefore, ModelMatrix is Mesh->Transform
		// converted to row-major.
		memcpy(&CurrentNode->ModelMatrix, TransformColMajor, sizeof(XMFLOAT4X4));

		if (NodeData->mesh) {
			ptrdiff_t Index = shgeti(MeshMap, NodeData->mesh->name);
			if (Index != -1) {
				CurrentNode->Mesh = MeshMap[Index].Mesh;
			}
		}
	}
	shfree(MeshMap);
}

cgltf_data *
GetData(PCWSTR Path, M_Arena *UploadArena)
{
	void *FileData = NULL;

	LONG Size = LoadGLTFFile(Path, UploadArena, &FileData);
	if (Size <= 0) {
		S_LogAppendf(L"Failed to load %s\n", Path);
		return NULL;
	}

	cgltf_options Options = {0};
	Options.memory.alloc_func = cgltf_arena_alloc;
	Options.memory.free_func = cgltf_arena_free;
	Options.memory.user_data = UploadArena;

	cgltf_data *Data = NULL;
	cgltf_result Result = cgltf_parse(&Options, FileData, Size, &Data);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to parse %s\n", Path);
		return NULL;
	}

	Result = LoadGLTFBuffers(&Options, UploadArena, Data, Path);
	if (Result != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to load GLTF buffers from %s\n", Path);
		return NULL;
	}
	return Data;
}

void
SetModelName(PCWSTR Path, R_Model *Model, M_Arena *Arena)
{
	WCHAR FileNameW[MAX_PATH];
	Win32GetFileNameOnly(Path, FileNameW, MAX_PATH);
	INT UTF8Size = UTF8_SIZE(FileNameW);
	Model->Name = M_ArenaAlloc(Arena, UTF8Size);
	W_TO_UTF8(FileNameW, Model->Name, UTF8Size);
}

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
LoadVerticesAndIndicesIntoBuffers(R_Core *Renderer,
								  R_Primitive *Primitive,
								  cgltf_accessor *PositionAccessor,
								  cgltf_accessor *NormalAccessor,
								  cgltf_accessor *IndicesAccessor,
								  cgltf_accessor *UVAccessorsData[2],
								  M_Arena *Arena)
{
	if (!PositionAccessor) {
		S_LogAppend(L"Mesh has no POSITION attribute\n");
		return;
	}

	UINT VertexCount = PositionAccessor->count;
	UINT VertexBufferSize = sizeof(R_Vertex) * VertexCount;
	R_Vertex *Vertices = M_ArenaAlloc(Arena, VertexBufferSize);

	for (UINT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++) {
		FLOAT Position[3];
		cgltf_accessor_read_float(PositionAccessor, VertexIndex, Position, 3);
		Vertices[VertexIndex].Position = (XMFLOAT3){Position[0], Position[1], Position[2]};

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
		}
	}

	size_t IndexCount = IndicesAccessor ? IndicesAccessor->count : 0;
	uint16_t *Indices = NULL;
	if (IndexCount > 0) {
		Indices = M_ArenaAlloc(Arena, sizeof(uint16_t) * IndexCount);
		for (size_t i = 0; i < IndexCount; i++) {
			uint32_t Index;
			cgltf_accessor_read_uint(IndicesAccessor, (int)i, &Index, 1);
			Indices[i] = (uint16_t)Index;
		}
	}

	memcpy(Renderer->UploadBufferCpuAddress + Renderer->CurrentUploadBufferOffset, Vertices, VertexBufferSize);
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {
	  .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->VertexBufferDefault) + Renderer->CurrentVertexBufferOffset,
	  .SizeInBytes = VertexBufferSize,
	  .StrideInBytes = sizeof(R_Vertex),
	};
	ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->VertexBufferDefault, Renderer->CurrentVertexBufferOffset,
											   Renderer->UploadBuffer, Renderer->CurrentUploadBufferOffset, VertexBufferSize);
	Renderer->CurrentVertexBufferOffset += VertexBufferSize;
	Renderer->CurrentUploadBufferOffset += VertexBufferSize;

	UINT IndexBufferSize = IndexCount * sizeof(UINT16);
	memcpy(Renderer->UploadBufferCpuAddress + Renderer->CurrentUploadBufferOffset, Indices, IndexBufferSize);
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->IndexBufferDefault) +
																 Renderer->CurrentIndexBufferOffset,
											   .Format = DXGI_FORMAT_R16_UINT,
											   .SizeInBytes = IndexBufferSize};
	ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->IndexBufferDefault, Renderer->CurrentIndexBufferOffset,
											   Renderer->UploadBuffer, Renderer->CurrentUploadBufferOffset, IndexBufferSize);
	Renderer->CurrentIndexBufferOffset += IndexBufferSize;
	Renderer->CurrentUploadBufferOffset += IndexBufferSize;

	Primitive->VertexBufferView = VertexBufferView;
	Primitive->IndexBufferView = IndexBufferView;
	Primitive->IndexCount = IndexCount;
}

void
PreloadImages(R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena *UploadArena)
{
	Model->Images = M_ArenaAlloc(UploadArena, Data->images_count * sizeof(R_Texture));
	Model->ImagesCount = Data->images_count;

	for (int i = 0; i < Data->images_count; ++i) {
		cgltf_image *BaseImage = &Data->images[i];
		size_t Size = 0;
		int Channels = 0;
		WCHAR BasePath[MAX_PATH];
		wcscpy_s(BasePath, MAX_PATH, Path);
		Win32RemoveAllAfterLastSlash(BasePath);

		if (ExtractImageData(BasePath, BaseImage, UploadArena, &Model->Images[i])) {
			Model->Images[i].Name = CreateTextureName(UploadArena, BaseImage, Path, i);
		}
	}
}

BOOL
ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_image *Img, _In_ M_Arena *UploadArena, _Out_ R_Texture *Texture)
{
	if (Texture == NULL) {
		return FALSE;
	}

	unsigned char *StbiData = NULL;

	if (Img->uri) {
		if (IsDataEmbedded(Img)) {
			const char *FirstCommaPtr = strchr(Img->uri, ',');
			if (FirstCommaPtr == NULL) {
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
			StbiData = stbi_load_from_memory(Decoded, (int)DecodedMaxCap, &Texture->Width, &Texture->Height, &Texture->Channels, 4);
			if (StbiData == NULL) {
				return FALSE;
			}
		} else {
			char FullPath[MAX_PATH];
			Win32AppendFileNameToPath(BasePath, Img->uri, FullPath);
			StbiData = stbi_load(FullPath, &Texture->Width, &Texture->Height, &Texture->Channels, 4);
			if (StbiData == NULL) {
				return FALSE;
			}
		}
	} else if (Img->buffer_view) {
		cgltf_buffer_view *BufferView = Img->buffer_view;
		cgltf_buffer *Buffer = BufferView->buffer;
		const UINT8 *Data = (const UINT8 *)Buffer->vertex_data + BufferView->offset;
		StbiData = stbi_load_from_memory(Data, (int)BufferView->size, &Texture->Width, &Texture->Height, &Texture->Channels, 4);
		if (StbiData == NULL) {
			return FALSE;
		}
	}

	if (StbiData == NULL) {
		return FALSE;
	}
	Texture->Size = (size_t)(Texture->Width) * (size_t)(Texture->Height) * 4;
	Texture->Pixels = M_ArenaAlloc(UploadArena, Texture->Size);
	memcpy(Texture->Pixels, StbiData, Texture->Size);
	stbi_image_free(StbiData);
	return TRUE;
}

cgltf_result
LoadGLTFBuffer(
	_In_z_ PCWSTR FullPath, _In_z_ PCWSTR BufferFileName, _In_ M_Arena *Arena, _Out_ size_t *Size, _Outptr_result_bytebuffer_(*Size) void **Data)
{
	WCHAR FullPathBuffer[MAX_PATH];
	wcscpy_s(FullPathBuffer, MAX_PATH, FullPath);
	Win32RemoveAllAfterLastSlash(FullPathBuffer);
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
			UTF8_TO_W(Uri, UriW, MAX_PATH);
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
		return 0;
	}

	return Size;
}

BOOL
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
		UTF8_TO_W(BaseImage->uri, UriW, MAX_PATH);
		swprintf_s(UniqueNameW, MAX_PATH, L"%s_%d_%s", Path, i, UriW);
	} else {
		swprintf_s(UniqueNameW, MAX_PATH, L"%s_Internal_%d", Path, i);
	}

	int UTF8Size = UTF8_SIZE(UniqueNameW);
	PSTR UniqueName = M_ArenaAlloc(UploadArena, UTF8Size);
	W_TO_UTF8(UniqueNameW, UniqueName, UTF8Size);
	return UniqueName;
}

void
LoadPBRData(R_Core *Renderer, const R_Texture *const Images, cgltf_image *ImagesData, cgltf_material *Material, R_PBRConstantBuffer *CB)
{
	CB->AlbedoTextureIndex = R_GetTextureIndex(Renderer, NULL);
	CB->NormalTextureIndex = R_GetTextureIndex(Renderer, NULL);
	CB->MetallicTextureIndex = R_GetTextureIndex(Renderer, NULL);
	CB->OcclusionTextureIndex = R_GetTextureIndex(Renderer, NULL);
	CB->EmissiveTextureIndex = R_GetTextureIndex(Renderer, NULL);

	if (Material) {
		if (Material->has_pbr_metallic_roughness) {
			cgltf_pbr_metallic_roughness *MetallicRoughnessData = &Material->pbr_metallic_roughness;
			memcpy(&CB->BaseColorFactor, MetallicRoughnessData->base_color_factor, sizeof(float) * 4);
			CB->MetallicFactor = MetallicRoughnessData->metallic_factor;
			CB->RoughnessFactor = MetallicRoughnessData->roughness_factor;

			if (MetallicRoughnessData->base_color_texture.texture) {
				UINT AlbedoIndex = MetallicRoughnessData->base_color_texture.texture->image - ImagesData;
				CB->AlbedoTextureIndex = R_GetTextureIndex(Renderer, &Images[AlbedoIndex]);

				if (MetallicRoughnessData->base_color_texture.has_transform) {
					cgltf_texture_transform *Transform = &MetallicRoughnessData->base_color_texture.transform;
					INT uvIndex = MetallicRoughnessData->base_color_texture.texcoord;
					CB->UVScale.x = Transform->scale[0];
					CB->UVScale.y = Transform->scale[1];
					CB->UVOffset.x = Transform->offset[0];
					CB->UVOffset.y = Transform->offset[1];
					CB->UVRotation = Transform->rotation;
				} else {
					CB->UVScale.x = 1.0f;
					CB->UVScale.y = 1.0f;
					CB->UVOffset.x = 0.0f;
					CB->UVOffset.y = 0.0f;
					CB->UVRotation = 0.0f;
				}
			}

			if (MetallicRoughnessData->metallic_roughness_texture.texture) {
				UINT MetallicIndex = MetallicRoughnessData->metallic_roughness_texture.texture->image - ImagesData;
				CB->MetallicTextureIndex = R_GetTextureIndex(Renderer, &Images[MetallicIndex]);
			}
		}
		cgltf_texture_view *NormalTextureView = &Material->normal_texture;
		if (NormalTextureView->texture) {
			UINT NormalIndex = NormalTextureView->texture->image - ImagesData;
			CB->NormalTextureIndex = R_GetTextureIndex(Renderer, &Images[NormalIndex]);
		}

		if (Material->occlusion_texture.texture) {
			UINT OcclusionIndex = Material->occlusion_texture.texture->image - ImagesData;
			CB->OcclusionTextureIndex = R_GetTextureIndex(Renderer, &Images[OcclusionIndex]);
		}

		memcpy(&CB->EmissiveFactor, Material->emissive_factor, sizeof(cgltf_float) * 3);
		if (Material->emissive_texture.texture) {
			UINT EmissiveIndex = Material->emissive_texture.texture->image - ImagesData;
			CB->EmissiveTextureIndex = R_GetTextureIndex(Renderer, &Images[EmissiveIndex]);
		}
	}
}