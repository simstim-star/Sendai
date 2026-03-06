#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../ui/ui.h"
#include "../win32/win_path.h"
#include "renderer.h"

#include "dxgidebug.h"
#include "render_types.h"

#include "../core/camera.h"
#include <stdint.h>

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void signal_and_wait(R_World *const renderer);
static void update_resource(ID3D12Resource *resource, void *data, size_t data_size);

void R_UploadTexture(R_World *renderer, R_Texture *source, ID3D12Resource **out_texture, D3D12_GPU_DESCRIPTOR_HANDLE *out_srv, UINT srv_index)
{
	D3D12_RESOURCE_DESC tex_desc = {
	  .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	  .Width = source->Width,
	  .Height = source->Height,
	  .DepthOrArraySize = 1,
	  .MipLevels = 1,
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc = {1, 0},
	  .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
	  .Flags = D3D12_RESOURCE_FLAG_NONE
	};

	D3D12_HEAP_PROPERTIES heap_default = { .Type = D3D12_HEAP_TYPE_DEFAULT };
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		renderer->Device, &heap_default, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, out_texture);
	ExitIfFailed(hr);

	UINT64 upload_size = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT num_rows;
	UINT64 row_size, total_bytes;
	ID3D12Device_GetCopyableFootprints(renderer->Device, &tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size, &upload_size);
	D3D12_RESOURCE_DESC upload_desc = CD3DX12_RESOURCE_DESC_BUFFER(upload_size, D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES heap_upload = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	ID3D12Resource *upload = NULL;
	hr = ID3D12Device_CreateCommittedResource(
		renderer->Device, &heap_upload, D3D12_HEAP_FLAG_NONE, &upload_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &upload);
	ExitIfFailed(hr);

	UINT8 *mapped = NULL;
	D3D12_RANGE range = {0, 0};
	ID3D12Resource_Map(upload, 0, &range, (void **)&mapped);
	for (UINT y = 0; y < num_rows; ++y) {
		memcpy(mapped + footprint.Offset + y * footprint.Footprint.RowPitch, source->Pixels + y * source->Width * 4, source->Width * 4);
	}
	ID3D12Resource_Unmap(upload, 0, NULL);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {.pResource = *out_texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
	D3D12_TEXTURE_COPY_LOCATION src_location = {.pResource = upload, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = footprint};
	ID3D12GraphicsCommandList_CopyTextureRegion(renderer->CommandList, &dst_location, 0, 0, 0, &src_location, NULL);
	D3D12_RESOURCE_BARRIER barrier = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition = {
		.pResource = *out_texture,
		.Subresource = 0,
		.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
		.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	  }};
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->CommandList, 1, &barrier);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = 1,
	};

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->SrvHeap, &cpu_desc_handle);
	cpu_desc_handle.ptr += srv_index * renderer->SrvDescriptorSize;
	ID3D12Device_CreateShaderResourceView(renderer->Device, *out_texture, &srv_desc, cpu_desc_handle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(renderer->SrvHeap, &gpu_desc_handle);
	*out_srv = gpu_desc_handle;
	out_srv->ptr += srv_index * renderer->SrvDescriptorSize;

	R_ExecuteCommands(renderer);
	ID3D12Resource_Release(upload);
}

/****************************************************
	Public functions
*****************************************************/

void R_Init(R_World *const renderer, HWND hwnd)
{
	renderer->hWnd = hwnd;
	renderer->AspectRatio = (float)(renderer->Width) / (renderer->Height);
	renderer->Viewport = (D3D12_VIEWPORT){0.0f, 0.0f, (float)(renderer->Width), (float)(renderer->Height)};
	renderer->ScissorRect = (D3D12_RECT){0, 0, (LONG)(renderer->Width), (LONG)(renderer->Height)};
	win32_curr_path(renderer->AssetsPath, _countof(renderer->AssetsPath));

	/* D3D12 setup */

	int is_debug_factory = 0;
#if defined(_DEBUG)
	ID3D12Debug1 *debug_controller = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void **)&debug_controller))) {
		ID3D12Debug1_EnableDebugLayer(debug_controller);
		// ID3D12Debug1_SetEnableGPUBasedValidation(debug_controller, 1);
		is_debug_factory |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1_Release(debug_controller);
	}
#endif

	IDXGIFactory2 *dxgi_factory = NULL;
	HRESULT hr = CreateDXGIFactory2(is_debug_factory, &IID_IDXGIFactory2, (void **)&dxgi_factory);
	ExitIfFailed(hr);

	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &renderer->Device);
	ExitIfFailed(hr);

	D3D12_COMMAND_QUEUE_DESC command_queue_desc = {
	  .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	  .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	  .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateCommandQueue(renderer->Device, &command_queue_desc, &IID_ID3D12CommandQueue, &renderer->CommandQueue);
	ExitIfFailed(hr);

	renderer->FenceValue = 0;
	hr = ID3D12Device_CreateFence(renderer->Device, renderer->FenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &renderer->Fence);
	ExitIfFailed(hr);

	hr = ID3D12Device_CreateCommandAllocator(renderer->Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, &renderer->CommandAllocator);
	ExitIfFailed(hr);

	D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  .NumDescriptors = 16,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  .NodeMask = 0,
	};

	hr = ID3D12Device_CreateDescriptorHeap(renderer->Device, &srv_heap_desc, &IID_ID3D12DescriptorHeap, &renderer->SrvHeap);
	ExitIfFailed(hr);

	renderer->SrvDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hr = ID3D12Device_CreateCommandList(
		renderer->Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer->CommandAllocator, renderer->PipelineStateScene, &IID_ID3D12GraphicsCommandList1,
		&renderer->CommandList);
	ExitIfFailed(hr);

	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	// Constant buffers must be 256-byte aligned size
	UINT cb_size = (sizeof(R_ConstantBuffer) + 255) & ~255;
	const D3D12_RESOURCE_DESC cb_desc = CD3DX12_RESOURCE_DESC_BUFFER(cb_size, D3D12_RESOURCE_FLAG_NONE, 0);

	hr = ID3D12Device_CreateCommittedResource(
		renderer->Device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &cb_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		&renderer->ConstantBuffer);
	ExitIfFailed(hr);

	renderer->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (renderer->FenceEvent == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		ExitIfFailed(hr);
	}

	D3D12_DESCRIPTOR_HEAP_DESC rtv_desc_heap_desc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	  .NumDescriptors = 2,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(renderer->Device, &rtv_desc_heap_desc, &IID_ID3D12DescriptorHeap, &renderer->RtvDescriptorHeap);
	ExitIfFailed(hr);

	renderer->RtvDescIncrement = ID3D12Device_GetDescriptorHandleIncrementSize(renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {
	  .Width = renderer->Width,
	  .Height = renderer->Height,
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .Stereo = 0,
	  .SampleDesc.Count = 1,
	  .SampleDesc.Quality = 0,
	  .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
	  .BufferCount = 2,
	  .Scaling = DXGI_SCALING_STRETCH,
	  .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
	  .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
	  .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
	};
	hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)renderer->CommandQueue, renderer->hWnd, &swap_chain_desc, NULL, NULL, &renderer->SwapChain);
	ExitIfFailed(hr);

	hr = IDXGISwapChain1_GetBuffer(renderer->SwapChain, 0, &IID_ID3D12Resource, &renderer->RtvBuffers[0]);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->SwapChain, 1, &IID_ID3D12Resource, &renderer->RtvBuffers[1]);
	ExitIfFailed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->RtvDescriptorHeap, &descriptor_handle);
	ID3D12Device_CreateRenderTargetView(renderer->Device, renderer->RtvBuffers[0], NULL, descriptor_handle);
	renderer->RtvHandles[0] = descriptor_handle;

	descriptor_handle.ptr += renderer->RtvDescIncrement;
	ID3D12Device_CreateRenderTargetView(renderer->Device, renderer->RtvBuffers[1], NULL, descriptor_handle);
	renderer->RtvHandles[1] = descriptor_handle;

	IDXGIFactory2_Release(dxgi_factory);
}

void R_Vertices(ID3D12Device *device, R_Mesh *const mesh)
{
	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	const D3D12_RESOURCE_DESC buffer_resource = CD3DX12_RESOURCE_DESC_BUFFER(sizeof(R_Vertex) * 2400000, D3D12_RESOURCE_FLAG_NONE, 0);

	// Note: using upload heaps to transfer static data like vert buffers is not
	// recommended. Every time the GPU needs it, the upload heap will be marshalled
	// over. Please read up on Default Heap usage. An upload heap is used here for
	// code simplicity and because there are very few verts to actually transfer
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &buffer_resource, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		&mesh->VertexBuffer);
	ExitIfFailed(hr);

	mesh->VertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(mesh->VertexBuffer);
	mesh->VertexBufferView.StrideInBytes = sizeof(R_Vertex);
	mesh->VertexBufferView.SizeInBytes = sizeof(R_Vertex) * 24;
}

void R_Indices(ID3D12Device *device, R_Mesh *const mesh)
{
	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	D3D12_RESOURCE_DESC ib_desc = CD3DX12_RESOURCE_DESC_BUFFER(mesh->IndexCount * sizeof(uint16_t), D3D12_RESOURCE_FLAG_NONE, 0);
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &ib_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		(void **)(&mesh->IndexBuffer));
	ExitIfFailed(hr);

	update_resource(mesh->IndexBuffer, mesh->Indices, mesh->IndexCount * sizeof(uint16_t));

	mesh->IndexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(mesh->IndexBuffer);
	mesh->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	mesh->IndexBufferView.SizeInBytes = (UINT)(mesh->IndexCount * sizeof(uint16_t));
}

void R_Update(R_World *const renderer, R_Camera *const camera, SendaiScene *scene)
{
	for (int i = 0; i < scene->MeshCount; ++i) {
		update_resource(scene->Meshes[i].VertexBuffer, scene->Meshes[i].Vertices, scene->Meshes[i].VertexCount * sizeof(R_Vertex));
	}

	XMMATRIX view = R_CameraViewMatrix(camera->Position, camera->LookDirection, camera->UpDirection);
	XMMATRIX proj = R_CameraProjectionMatrix(XM_PIDIV4, renderer->AspectRatio, 0.1f, 1000.0f);
	XMMATRIX mvp = XM_MAT_MULT(view, proj);
	mvp = XM_MAT_TRANSP(mvp);
	update_resource(renderer->ConstantBuffer, &mvp, sizeof(XMMATRIX));
}

void R_Draw(R_World *const renderer, SendaiScene *scene)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(renderer->CommandList, scene->RootSign);
	ID3D12GraphicsCommandList_RSSetViewports(renderer->CommandList, 1, &renderer->Viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(renderer->CommandList, 1, &renderer->ScissorRect);

	D3D12_RESOURCE_BARRIER resource_barrier = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition.pResource = renderer->RtvBuffers[renderer->RtvIndex],
	  .Transition.Subresource = 0,
	  .Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
	  .Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
	  .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
	};
	const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->CommandList, 1, &resource_barrier);
	ID3D12GraphicsCommandList_ClearRenderTargetView(renderer->CommandList, renderer->RtvHandles[renderer->RtvIndex], clearColor, 0, NULL);
	ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->CommandList, 1, &renderer->RtvHandles[renderer->RtvIndex], FALSE, NULL);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(renderer->CommandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(renderer->CommandList, 0, ID3D12Resource_GetGPUVirtualAddress(renderer->ConstantBuffer));
	ID3D12DescriptorHeap *heaps[] = {renderer->SrvHeap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(renderer->CommandList, 1, heaps);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(renderer->CommandList, 1, renderer->ModelGpuSrv);
	for (int i = 0; i < scene->MeshCount; ++i) {
		ID3D12GraphicsCommandList_IASetVertexBuffers(renderer->CommandList, 0, 1, &scene->Meshes[i].VertexBufferView);
		ID3D12GraphicsCommandList_IASetIndexBuffer(renderer->CommandList, &scene->Meshes[i].IndexBufferView);
		ID3D12GraphicsCommandList_DrawIndexedInstanced(renderer->CommandList, scene->Meshes[i].IndexCount, 1, 0, 0, 0);
	}
	UI_Draw(renderer->CommandList);

	// Bring the rtv resource back to present state
	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Transition.pResource = renderer->RtvBuffers[renderer->RtvIndex];
	resource_barrier.Transition.Subresource = 0;
	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->CommandList, 1, &resource_barrier);

	R_ExecuteCommands(renderer);

	HRESULT hr = IDXGISwapChain2_Present(renderer->SwapChain, 1, 0);
	renderer->RtvIndex = (renderer->RtvIndex + 1) % FRAME_COUNT;
	if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
		MessageBox(NULL, L"D3D12 device is lost or removed!", L"Error", 0);
		return;
	}
	ExitIfFailed(hr);
}

void R_ExecuteCommands(R_World *const renderer)
{
	ID3D12GraphicsCommandList_Close(renderer->CommandList);
	ID3D12CommandList *cmd_lists[] = {(ID3D12CommandList *)renderer->CommandList};
	ID3D12CommandQueue_ExecuteCommandLists(renderer->CommandQueue, 1, cmd_lists);
	signal_and_wait(renderer);
	ID3D12CommandAllocator_Reset(renderer->CommandAllocator);
	ID3D12GraphicsCommandList_Reset(renderer->CommandList, renderer->CommandAllocator, renderer->PipelineStateScene);
}

void R_Destroy(R_World *renderer)
{
	UI_Destroy();
	for (int i = 0; i < FRAME_COUNT; ++i) {
		signal_and_wait(renderer);
		ID3D12Resource_Release(renderer->RtvBuffers[i]);
	}
	ID3D12DescriptorHeap_Release(renderer->RtvDescriptorHeap);
	IDXGISwapChain1_Release(renderer->SwapChain);
	ID3D12GraphicsCommandList_Release(renderer->CommandList);
	ID3D12CommandAllocator_Release(renderer->CommandAllocator);
	ID3D12CommandQueue_Release(renderer->CommandQueue);
	ID3D12Fence_Release(renderer->Fence);
	ID3D12PipelineState_Release(renderer->PipelineStateScene);
	ID3D12Resource_Release(renderer->ConstantBuffer);
	ID3D12Device_Release(renderer->Device);
	ID3D12Device_Release(renderer->SrvHeap);
	ID3D12Device_Release(renderer->ModelGpuTexture);
	CloseHandle(renderer->FenceEvent);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (void **)&debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}

void R_SwapchainResize(R_World *const renderer, int width, int height)
{
	for (int i = 0; i < FRAME_COUNT; ++i) {
		signal_and_wait(renderer);
		ID3D12Resource_Release(renderer->RtvBuffers[i]);
	}

	HRESULT hr = IDXGISwapChain1_ResizeBuffers(renderer->SwapChain, 2, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->SwapChain, 0, &IID_ID3D12Resource, &renderer->RtvBuffers[0]);
	ExitIfFailed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->SwapChain, 1, &IID_ID3D12Resource, &renderer->RtvBuffers[1]);
	ExitIfFailed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->RtvDescriptorHeap, &descriptor_handle);
	ID3D12Device_CreateRenderTargetView(renderer->Device, renderer->RtvBuffers[0], NULL, descriptor_handle);
	renderer->RtvHandles[0] = descriptor_handle;
	descriptor_handle.ptr += renderer->RtvDescIncrement;
	ID3D12Device_CreateRenderTargetView(renderer->Device, renderer->RtvBuffers[1], NULL, descriptor_handle);
	renderer->RtvHandles[1] = descriptor_handle;
	renderer->RtvIndex = 0;
}

/****************************************************
	Implementation of private functions
*****************************************************/

static void signal_and_wait(R_World *const renderer)
{
	HRESULT hr = ID3D12CommandQueue_Signal(renderer->CommandQueue, renderer->Fence, ++renderer->FenceValue);
	ExitIfFailed(hr);
	ID3D12Fence_SetEventOnCompletion(renderer->Fence, renderer->FenceValue, renderer->FenceEvent);
	WaitForSingleObject(renderer->FenceEvent, INFINITE);
}

void update_resource(ID3D12Resource *resource, void *data, size_t data_size)
{
	UINT8 *begin = NULL;
	const D3D12_RANGE read_range = {0, 0};
	HRESULT hr = ID3D12Resource_Map(resource, 0, &read_range, (void **)&begin);
	ExitIfFailed(hr);
	memcpy(begin, data, data_size);
	ID3D12Resource_Unmap(resource, 0, NULL);
}
