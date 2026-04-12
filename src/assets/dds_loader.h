/* This is a C port of https://github.com/microsoft/DirectXTex/blob/main/DDSTextureLoader/DDSTextureLoader12.h */

//--------------------------------------------------------------------------------------
// File: DDSTextureLoader12.h
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include <d3d12.h>
#pragma comment(lib, "dxguid.lib")

#ifndef DDS_ALPHA_MODE_DEFINED
#define DDS_ALPHA_MODE_DEFINED

typedef enum DDS_ALPHA_MODE {
	DDS_ALPHA_MODE_UNKNOWN = 0,
	DDS_ALPHA_MODE_STRAIGHT = 1,
	DDS_ALPHA_MODE_PREMULTIPLIED = 2,
	DDS_ALPHA_MODE_OPAQUE = 3,
	DDS_ALPHA_MODE_CUSTOM = 4,
} DDS_ALPHA_MODE;

#endif

#ifndef DDS_LOADER_FLAGS_DEFINED
#define DDS_LOADER_FLAGS_DEFINED

typedef enum DDS_LOADER_FLAGS {
	DDS_LOADER_DEFAULT = 0,
	DDS_LOADER_FORCE_SRGB = 0x1,
	DDS_LOADER_IGNORE_SRGB = 0x2,
	DDS_LOADER_MIP_RESERVE = 0x8,
} DDS_LOADER_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS(DDS_LOADER_FLAGS);

#endif

// Standard version
HRESULT __cdecl LoadDDSTextureFromMemory(_In_ ID3D12Device *d3dDevice,
										 _In_reads_bytes_(ddsDataSize) const UINT8 *ddsData,
										 size_t ddsDataSize,
										 _Outptr_ ID3D12Resource **texture,
										 D3D12_SUBRESOURCE_DATA *subresources,
										 size_t maxsize,
										 _Out_opt_ DDS_ALPHA_MODE *alphaMode,
										 _Out_opt_ BOOL *isCubeMap);

HRESULT __cdecl LoadDDSTextureFromMemoryEx(ID3D12Device *d3dDevice,
						   const UINT8 *ddsData,
						   size_t ddsDataSize,
						   size_t maxsize,
						   D3D12_RESOURCE_FLAGS resFlags,
						   DDS_LOADER_FLAGS loadFlags,
						   ID3D12Resource **texture,
						   D3D12_SUBRESOURCE_DATA *subresources,
						   DDS_ALPHA_MODE *alphaMode,
						   BOOL *isCubeMap);

HRESULT __cdecl LoadDDSTextureFromFile(_In_ ID3D12Device *d3dDevice,
									   _In_z_ const wchar_t *szFileName,
									   _Outptr_ ID3D12Resource **texture,
									   UINT8 **ddsData,
									   D3D12_SUBRESOURCE_DATA *subresources,
									   size_t maxsize,
									   _Out_opt_ DDS_ALPHA_MODE *alphaMode,
									   _Out_opt_ BOOL *isCubeMap);

HRESULT __cdecl LoadDDSTextureFromFileEx(ID3D12Device *d3dDevice,
						 const wchar_t *fileName,
						 size_t maxsize,
						 D3D12_RESOURCE_FLAGS resFlags,
						 DDS_LOADER_FLAGS loadFlags,
						 ID3D12Resource **texture,
						 UINT8 **ddsData,
						 D3D12_SUBRESOURCE_DATA *subresources,
						 DDS_ALPHA_MODE *alphaMode,
						 BOOL *isCubeMap);