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
#include "../win32/str_helper.h"
#include "../win32/win_path.h"
#include "dds_loader.h"

/****************************************************
	Helper structs and enums
*****************************************************/

typedef struct MeshLookup {
	cgltf_mesh *key;
	R_Mesh *value;
} MeshLookup;

typedef enum E_TextureFormat {
	ETF_DDS,
	ETF_PNG,
} E_TextureFormat;

/****************************************************
	Forward declaration of private functions
*****************************************************/

static cgltf_data *GetData(PCWSTR Path, M_Arena *UploadArena);

static void SetModelName(PCWSTR Path, R_Model *Model, M_Arena *Arena);

static void LoadNodes(R_Core *Renderer, R_Model *Model, cgltf_data *Data, M_Arena *SceneArena, M_Arena *UploadArena);

static void PreloadImages(R_Core *Renderer, R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena *UploadArena);

static BOOL ExtractImageData(_In_z_ WCHAR BasePath[MAX_PATH], _In_ cgltf_image *Img, _In_ M_Arena *UploadArena, _Out_ R_Texture *Texture);

static LONG LoadGLTFFile(_In_z_ PCWSTR Path, _In_ M_Arena *Arena, _Outptr_ void **Data);

static cgltf_result LoadGLTFBuffer(_In_z_ PCWSTR FullPath,
								   _In_z_ PCWSTR BufferFileName,
								   _In_ M_Arena *Arena,
								   _Out_ size_t *Size,
								   _Outptr_result_bytebuffer_(*Size) void **Data);

static PSTR CreateTextureName(M_Arena *UploadArena, cgltf_image *BaseImage, PCWSTR Path, int i);

static BOOL IsDataEmbedded(const cgltf_image *const BaseImage);

static void RetriveAttributeData(cgltf_primitive *PrimitiveData,
								 cgltf_accessor *UVAccessorsData[2],
								 cgltf_accessor *AccessorsData[cgltf_attribute_type_max_enum]);

static void LoadPBRData(R_Core *Renderer, R_Texture *const Images, cgltf_image *ImagesData, cgltf_material *Material, R_PBRConstantBuffer *CB);

static void LoadVerticesAndIndicesIntoBuffers(R_Core *Renderer,
											  R_Primitive *Primitive,
											  cgltf_accessor *PositionAccessor,
											  cgltf_accessor *NormalAccessor,
											  cgltf_accessor *TangentAccessor,
											  cgltf_accessor *IndicesAccessor,
											  cgltf_accessor *UVAccessorsData[2],
											  M_Arena *Arena);

static XMFLOAT4 *ComputeTangents(cgltf_accessor *PositionAccessor,
								 cgltf_accessor *NormalAccessor,
								 cgltf_accessor *UVAccessor,
								 cgltf_accessor *IndicesAccessor,
								 M_Arena *Arena);

static VOID GetIndexAndFormatFromTexture(cgltf_texture *Texture, cgltf_image *Images, int *Index, E_TextureFormat *Format);

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
		PreloadImages(Renderer, Model, Data, Path, &Scene->UploadArena);
	}

	R_GenerateMips(Model, &Scene->UploadArena);
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

			LoadVerticesAndIndicesIntoBuffers(Renderer, Primitive, Accessors[cgltf_attribute_type_position],
											  Accessors[cgltf_attribute_type_normal], Accessors[cgltf_attribute_type_tangent],
											  PrimitiveData->indices, UVAccessorsData, UploadArena);
			LoadPBRData(Renderer, Model->Images, Data->images, PrimitiveData->material, &Primitive->ConstantBuffer);
		}

		MeshLookup Lookup = {.key = MeshData, .value = CurrentMesh};
		hmput(MeshMap, MeshData, CurrentMesh);
	}

	Model->Nodes = M_ArenaAlloc(SceneArena, Data->nodes_count * sizeof(R_Node));
	Model->NodesCount = 0;

	for (; Model->NodesCount < Data->nodes_count; Model->NodesCount++) {
		UINT NodeIndex = Model->NodesCount;
		cgltf_node *NodeData = &Data->nodes[NodeIndex];
		R_Node *CurrentNode = &Model->Nodes[NodeIndex];

		// Note: Mesh->Transform is col-major, but XMLoadFloat4x4 expects row-major.
		// This way, the matrix is automatically transposed already, because XMLoadFloat4x4
		// will pick as row what is col and vice-versa. Therefore, ModelMatrix is Mesh->Transform
		// converted to row-major.
		cgltf_float TransformColMajor[4][4];
		cgltf_node_transform_world(NodeData, TransformColMajor);
		/* Corrections for left-hand system */
		TransformColMajor[3][2] *= -1.0f;
		TransformColMajor[0][2] *= -1.0f;
		TransformColMajor[1][2] *= -1.0f;
		TransformColMajor[2][0] *= -1.0f;
		TransformColMajor[2][1] *= -1.0f;
		memcpy(&CurrentNode->ModelMatrix, TransformColMajor, sizeof(XMFLOAT4X4));

		if (NodeData->mesh) {
			CurrentNode->Mesh = hmget(MeshMap, NodeData->mesh);
		}
	}
	hmfree(MeshMap);
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
	if (cgltf_parse(&Options, FileData, Size, &Data) != cgltf_result_success) {
		if (Data) {
			cgltf_free(Data);
		}
		S_LogAppendf(L"Failed to parse %s\n", Path);
		return NULL;
	}

	if (LoadGLTFBuffers(&Options, UploadArena, Data, Path) != cgltf_result_success) {
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
RetriveAttributeData(cgltf_primitive *PrimitiveData,
					 cgltf_accessor *UVAccessorsData[2],
					 cgltf_accessor *AccessorsData[cgltf_attribute_type_max_enum])
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
								  cgltf_accessor *TangentAccessor,
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
	XMFLOAT4 *Tangents = NULL;
	if (PositionAccessor && NormalAccessor && UVAccessorsData[0] && IndicesAccessor) {
		Tangents = ComputeTangents(PositionAccessor, NormalAccessor, UVAccessorsData[0], IndicesAccessor, Arena);
	}

	for (UINT VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++) {
		FLOAT Position[3];
		cgltf_accessor_read_float(PositionAccessor, VertexIndex, Position, 3);
		Vertices[VertexIndex].Position = (XMFLOAT3){Position[0], Position[1], -Position[2]};

		if (NormalAccessor) {
			FLOAT Normal[3];
			cgltf_accessor_read_float(NormalAccessor, VertexIndex, Normal, 3);
			Vertices[VertexIndex].Normal.x = Normal[0];
			Vertices[VertexIndex].Normal.y = Normal[1];
			Vertices[VertexIndex].Normal.z = -Normal[2];
		}

		if (TangentAccessor) {
			FLOAT Tangent[4];
			cgltf_accessor_read_float(TangentAccessor, VertexIndex, Tangent, 4);
			Vertices[VertexIndex].Tangent.x = Tangent[0];
			Vertices[VertexIndex].Tangent.y = Tangent[1];
			Vertices[VertexIndex].Tangent.z = -Tangent[2];
			Vertices[VertexIndex].Tangent.w = -Tangent[3];
		} else if (Tangents) {
			Vertices[VertexIndex].Tangent.x = Tangents[VertexIndex].x;
			Vertices[VertexIndex].Tangent.y = Tangents[VertexIndex].y;
			Vertices[VertexIndex].Tangent.z = Tangents[VertexIndex].z;
			Vertices[VertexIndex].Tangent.w = Tangents[VertexIndex].w;
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
	void *Indices = NULL;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferSize = IndexCount * sizeof(UINT16);
	UINT IndexStride = sizeof(uint16_t);

	if (IndexCount > 0) {
		if (VertexCount > UINT16_MAX) {
			IndexStride = sizeof(uint32_t);
			IndexFormat = DXGI_FORMAT_R32_UINT;
			IndexBufferSize = IndexCount * sizeof(UINT32);
		}

		Indices = M_ArenaAlloc(Arena, IndexStride * IndexCount);

		for (size_t i = 0; i < IndexCount; i += 3) {
			uint32_t i0, i1, i2;
			cgltf_accessor_read_uint(IndicesAccessor, i, &i0, 1);
			cgltf_accessor_read_uint(IndicesAccessor, i + 1, &i1, 1);
			cgltf_accessor_read_uint(IndicesAccessor, i + 2, &i2, 1);

			/* Corrections for left-hand system */
			if (IndexStride == sizeof(uint16_t)) {
				((uint16_t *)Indices)[i] = (uint16_t)i0;
				((uint16_t *)Indices)[i + 1] = (uint16_t)i2;
				((uint16_t *)Indices)[i + 2] = (uint16_t)i1;
			} else {
				((uint32_t *)Indices)[i] = i0;
				((uint32_t *)Indices)[i + 1] = i2;
				((uint32_t *)Indices)[i + 2] = i1;
			}
		}
	}

	memcpy(Renderer->UploadBufferCpuAddress + Renderer->CurrentUploadBufferOffset, Vertices, VertexBufferSize);
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {
	  .BufferLocation = M_GpuAddress(Renderer->VertexBufferDefault, Renderer->CurrentVertexBufferOffset),
	  .SizeInBytes = VertexBufferSize,
	  .StrideInBytes = sizeof(R_Vertex),
	};
	ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->VertexBufferDefault, Renderer->CurrentVertexBufferOffset,
											   Renderer->UploadBuffer, Renderer->CurrentUploadBufferOffset, VertexBufferSize);
	Renderer->CurrentVertexBufferOffset += VertexBufferSize;
	Renderer->CurrentUploadBufferOffset += VertexBufferSize;

	/* Ensure alignment for both 2 and 4 bytes indices */
	Renderer->CurrentUploadBufferOffset = ROUND_UP_POWER_OF_2(Renderer->CurrentUploadBufferOffset, 4);
	Renderer->CurrentIndexBufferOffset = ROUND_UP_POWER_OF_2(Renderer->CurrentIndexBufferOffset, 4);
	memcpy(Renderer->UploadBufferCpuAddress + Renderer->CurrentUploadBufferOffset, Indices, IndexBufferSize);
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {.BufferLocation = M_GpuAddress(Renderer->IndexBufferDefault, Renderer->CurrentIndexBufferOffset),
											   .Format = IndexFormat,
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
PreloadImages(R_Core *Renderer, R_Model *Model, cgltf_data *Data, PCWSTR Path, M_Arena *UploadArena)
{
	Model->Images = M_ArenaAlloc(UploadArena, Data->images_count * sizeof(R_Texture));
	Model->ImagesCount = Data->images_count;

	for (INT i = 0; i < Data->textures_count; ++i) {
		INT ImageIndex;
		E_TextureFormat Format;
		GetIndexAndFormatFromTexture(&Data->textures[i], Data->images, &ImageIndex, &Format);
		cgltf_image *BaseImage = &Data->images[ImageIndex];
		switch (Format) {
		case ETF_PNG: {
			WCHAR BasePath[MAX_PATH];
			wcscpy_s(BasePath, MAX_PATH, Path);
			Win32RemoveAllAfterLastSlash(BasePath);

			if (ExtractImageData(BasePath, BaseImage, UploadArena, &Model->Images[i])) {
				Model->Images[i].Name = CreateTextureName(UploadArena, BaseImage, Path, i);
			}
			break;
		}
		case ETF_DDS: {
			WCHAR BasePath[MAX_PATH];

			wcscpy_s(BasePath, MAX_PATH, Path);
			Win32RemoveAllAfterLastSlash(BasePath);
			
			CHAR FullPath[MAX_PATH];
			Win32AppendFileNameToPath(BasePath, BaseImage->uri, FullPath);
			WCHAR FullPathW[MAX_PATH];
			UTF8_TO_W(FullPath, FullPathW, MAX_PATH);
			
			R_UploadTextureFromDDSFile(Renderer, FullPathW, BaseImage->uri);
			break;
		}
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
	Texture->MipLevels = R_CalculateMipLevels(Texture->Width, Texture->Height);
	Texture->MipPixels[0] = M_ArenaAlloc(UploadArena, Texture->Size);
	memcpy(Texture->MipPixels[0], StbiData, Texture->Size);

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
LoadPBRData(R_Core *Renderer, R_Texture *const Images, cgltf_image *ImagesData, cgltf_material *Material, R_PBRConstantBuffer *CB)
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
				INT AlbedoIndex = 0;
				GetIndexAndFormatFromTexture(MetallicRoughnessData->base_color_texture.texture, ImagesData, &AlbedoIndex, NULL);
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
				INT MetallicIndex = 0;
				GetIndexAndFormatFromTexture(MetallicRoughnessData->metallic_roughness_texture.texture, ImagesData, &MetallicIndex, NULL);
				CB->MetallicTextureIndex = R_GetTextureIndex(Renderer, &Images[MetallicIndex]);
			}
		}
		cgltf_texture_view *NormalTextureView = &Material->normal_texture;
		if (NormalTextureView->texture) {
			INT NormalIndex = 0;
			GetIndexAndFormatFromTexture(NormalTextureView->texture, ImagesData, &NormalIndex, NULL);
			CB->NormalTextureIndex = R_GetTextureIndex(Renderer, &Images[NormalIndex]);
		}

		if (Material->occlusion_texture.texture) {
			INT OcclusionIndex = 0;
			GetIndexAndFormatFromTexture(Material->occlusion_texture.texture, ImagesData, &OcclusionIndex, NULL);
			CB->OcclusionTextureIndex = R_GetTextureIndex(Renderer, &Images[OcclusionIndex]);
		}

		memcpy(&CB->EmissiveFactor, Material->emissive_factor, sizeof(cgltf_float) * 3);
		if (Material->emissive_texture.texture) {
			INT EmissiveIndex = 0;
			GetIndexAndFormatFromTexture(Material->emissive_texture.texture, ImagesData, &EmissiveIndex, NULL);
			CB->EmissiveTextureIndex = R_GetTextureIndex(Renderer, &Images[EmissiveIndex]);
		}
	}
}

// Based on https://github.com/salvatorespoto/gLTFViewer/blob/master/DX12Engine/Source/Utils/Cpp/GLTFSceneLoader.cpp#L508
XMFLOAT4 *
ComputeTangents(cgltf_accessor *PositionAccessor,
				cgltf_accessor *NormalAccessor,
				cgltf_accessor *UVAccessor,
				cgltf_accessor *IndicesAccessor,
				M_Arena *Arena)
{
	size_t VertexCount = PositionAccessor->count;
	XMFLOAT3 *Tan1 = M_ArenaAlloc(Arena, VertexCount * sizeof(XMFLOAT3));
	XMFLOAT3 *Tan2 = M_ArenaAlloc(Arena, VertexCount * sizeof(XMFLOAT3));
	XMFLOAT4 *FinalTangents = M_ArenaAlloc(Arena, VertexCount * sizeof(XMFLOAT4));

	for (size_t i = 0; i < IndicesAccessor->count; i += 3) {
		uint32_t i1 = (uint32_t)cgltf_accessor_read_index(IndicesAccessor, i);
		uint32_t i2 = (uint32_t)cgltf_accessor_read_index(IndicesAccessor, i + 1);
		uint32_t i3 = (uint32_t)cgltf_accessor_read_index(IndicesAccessor, i + 2);

		XMFLOAT3 v1, v2, v3;
		XMFLOAT2 w1, w2, w3;

		cgltf_accessor_read_float(PositionAccessor, i1, &v1.x, 3);
		cgltf_accessor_read_float(PositionAccessor, i2, &v2.x, 3);
		cgltf_accessor_read_float(PositionAccessor, i3, &v3.x, 3);

		cgltf_accessor_read_float(UVAccessor, i1, &w1.x, 2);
		cgltf_accessor_read_float(UVAccessor, i2, &w2.x, 2);
		cgltf_accessor_read_float(UVAccessor, i3, &w3.x, 2);

		float x1 = v2.x - v1.x;
		float x2 = v3.x - v1.x;
		float y1 = v2.y - v1.y;
		float y2 = v3.y - v1.y;
		float z1 = v2.z - v1.z;
		float z2 = v3.z - v1.z;

		float s1 = w2.x - w1.x;
		float s2 = w3.x - w1.x;
		float t1 = w2.y - w1.y;
		float t2 = w3.y - w1.y;

		float r = 1.0f / (s1 * t2 - s2 * t1);

		XMFLOAT3 sdir = (XMFLOAT3){(t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r};
		XMFLOAT3 tdir = (XMFLOAT3){(s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r};

		Tan1[i1] = (XMFLOAT3){Tan1[i1].x + sdir.x, Tan1[i1].y + sdir.y, Tan1[i1].z + sdir.z};
		Tan1[i2] = (XMFLOAT3){Tan1[i2].x + sdir.x, Tan1[i2].y + sdir.y, Tan1[i2].z + sdir.z};
		Tan1[i3] = (XMFLOAT3){Tan1[i3].x + sdir.x, Tan1[i3].y + sdir.y, Tan1[i3].z + sdir.z};

		Tan2[i1] = (XMFLOAT3){Tan2[i1].x + tdir.x, Tan2[i1].y + tdir.y, Tan2[i1].z + tdir.z};
		Tan2[i2] = (XMFLOAT3){Tan2[i2].x + tdir.x, Tan2[i2].y + tdir.y, Tan2[i2].z + tdir.z};
		Tan2[i3] = (XMFLOAT3){Tan2[i3].x + tdir.x, Tan2[i3].y + tdir.y, Tan2[i3].z + tdir.z};
	}

	for (size_t i = 0; i < VertexCount; i++) {
		XMFLOAT3 N;
		cgltf_accessor_read_float(NormalAccessor, i, &N.x, 3);

		XMVECTOR N_XMVECTOR = XMLoadFloat3(&N);
		XMVECTOR Tan1_XMVECTOR = XMLoadFloat3(&Tan1[i]);
		XMVECTOR Tan2_XMVECTOR = XMLoadFloat3(&Tan2[i]);

		// Gram-Schmidt orthogonalize
		XMVECTOR NdotTan1 = XM_VEC3_DOT(N_XMVECTOR, Tan1_XMVECTOR);
		XMVECTOR NxNT = XM_VEC_MULT(N_XMVECTOR, NdotTan1);
		XMVECTOR TminusNNT = XM_VEC_SUBTRACT(Tan1_XMVECTOR, NxNT);
		XMVECTOR TangetVector = XM_VEC3_NORM(TminusNNT);

		// Calculate handedness (w)
		XMVECTOR crossNT = XM_VEC3_CROSS(N_XMVECTOR, Tan1_XMVECTOR);
		XMVECTOR Dot = XM_VEC3_DOT(crossNT, Tan2_XMVECTOR);
		float Handedness = (XM_VECX(Dot) < 0.0f) ? -1.0f : 1.0f;
		TangetVector = XM_VEC_SETW(TangetVector, Handedness);
		XM_STORE_FLOAT4(&FinalTangents[i], TangetVector);
	}

	return FinalTangents;
}

VOID
GetIndexAndFormatFromTexture(cgltf_texture *Texture, cgltf_image *Image, int *OutIndex, E_TextureFormat *OutFormat)
{
	if (!Texture || !Image) {
		return -1;
	}

	for (cgltf_size i = 0; i < Texture->extensions_count; ++i) {
		if (strcmp(Texture->extensions[i].name, "MSFT_texture_dds") == 0) {
			// MSFT_texture_dds data contains the index of the DDS image
			// Structure is usually {"source": <image_index>}
			const PSTR pSource = strstr(Texture->extensions[i].vertex_data, "\"source\":");
			if (pSource) {
				if (OutIndex) {
					*OutIndex = atoi(pSource + 9);
				}
				if (OutFormat) {
					*OutFormat = ETF_DDS;
				}
				return;
			}
		}
	}

	if (OutIndex) {
		*OutIndex = (INT)(Texture->image - Image);
	}
	if (OutFormat) {
		*OutFormat = ETF_PNG;
	}
}