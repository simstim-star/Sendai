#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../gui/gui.h"
#include "../win32/win_path.h"
#include "renderer.h"

#include "dxgidebug.h"
#include "render_types.h"

#include "../core/camera.h"
#include <stdint.h>

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void signal_and_wait(Sendai_WorldRenderer *const renderer);
static void update_resource(ID3D12Resource *resource, void *data, size_t data_size);

void SendaiRenderer_upload_texture(Sendai_WorldRenderer *renderer, Sendai_Texture *source, ID3D12Resource **out_texture, D3D12_GPU_DESCRIPTOR_HANDLE *out_srv, UINT srv_index)
{
	D3D12_RESOURCE_DESC tex_desc = {
	  .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	  .Width = source->width,
	  .Height = source->height,
	  .DepthOrArraySize = 1,
	  .MipLevels = 1,
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .SampleDesc = {1, 0},
	  .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
	  .Flags = D3D12_RESOURCE_FLAG_NONE
	};

	D3D12_HEAP_PROPERTIES heap_default = { .Type = D3D12_HEAP_TYPE_DEFAULT };
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		renderer->device, &heap_default, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, out_texture);
	exit_if_failed(hr);

	UINT64 upload_size = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT num_rows;
	UINT64 row_size, total_bytes;
	ID3D12Device_GetCopyableFootprints(renderer->device, &tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size, &upload_size);
	D3D12_RESOURCE_DESC upload_desc = CD3DX12_RESOURCE_DESC_BUFFER(upload_size, D3D12_RESOURCE_FLAG_NONE, 0);
	D3D12_HEAP_PROPERTIES heap_upload = {.Type = D3D12_HEAP_TYPE_UPLOAD};
	ID3D12Resource *upload = NULL;
	hr = ID3D12Device_CreateCommittedResource(
		renderer->device, &heap_upload, D3D12_HEAP_FLAG_NONE, &upload_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &upload);
	exit_if_failed(hr);

	UINT8 *mapped = NULL;
	D3D12_RANGE range = {0, 0};
	ID3D12Resource_Map(upload, 0, &range, (void **)&mapped);
	for (UINT y = 0; y < num_rows; ++y) {
		memcpy(mapped + footprint.Offset + y * footprint.Footprint.RowPitch, source->pixels + y * source->width * 4, source->width * 4);
	}
	ID3D12Resource_Unmap(upload, 0, NULL);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {.pResource = *out_texture, .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0};
	D3D12_TEXTURE_COPY_LOCATION src_location = {.pResource = upload, .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = footprint};
	ID3D12GraphicsCommandList_CopyTextureRegion(renderer->command_list, &dst_location, 0, 0, 0, &src_location, NULL);
	D3D12_RESOURCE_BARRIER barrier = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition = {
		.pResource = *out_texture,
		.Subresource = 0,
		.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
		.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	  }};
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->command_list, 1, &barrier);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
	  .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
	  .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
	  .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
	  .Texture2D.MipLevels = 1,
	};

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->srv_heap, &cpu_desc_handle);
	cpu_desc_handle.ptr += srv_index * renderer->srv_descriptor_size;
	ID3D12Device_CreateShaderResourceView(renderer->device, *out_texture, &srv_desc, cpu_desc_handle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle;
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(renderer->srv_heap, &gpu_desc_handle);
	*out_srv = gpu_desc_handle;
	out_srv->ptr += srv_index * renderer->srv_descriptor_size;

	SendaiRenderer_execute_commands(renderer);
	ID3D12Resource_Release(upload);
}

/****************************************************
	Public functions
*****************************************************/

void SendaiRenderer_init(Sendai_WorldRenderer *const renderer, HWND hwnd)
{
	renderer->hwnd = hwnd;
	renderer->aspect_ratio = (float)(renderer->width) / (renderer->height);
	renderer->viewport = (D3D12_VIEWPORT){0.0f, 0.0f, (float)(renderer->width), (float)(renderer->height)};
	renderer->scissor_rect = (D3D12_RECT){0, 0, (LONG)(renderer->width), (LONG)(renderer->height)};
	win32_curr_path(renderer->assets_path, _countof(renderer->assets_path));

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
	exit_if_failed(hr);

	hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &renderer->device);
	exit_if_failed(hr);

	D3D12_COMMAND_QUEUE_DESC command_queue_desc = {
	  .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	  .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	  .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateCommandQueue(renderer->device, &command_queue_desc, &IID_ID3D12CommandQueue, &renderer->command_queue);
	exit_if_failed(hr);

	renderer->fence_value = 0;
	hr = ID3D12Device_CreateFence(renderer->device, renderer->fence_value, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &renderer->fence);
	exit_if_failed(hr);

	hr = ID3D12Device_CreateCommandAllocator(renderer->device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, &renderer->command_allocator);
	exit_if_failed(hr);

	D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  .NumDescriptors = 16,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  .NodeMask = 0,
	};

	hr = ID3D12Device_CreateDescriptorHeap(renderer->device, &srv_heap_desc, &IID_ID3D12DescriptorHeap, &renderer->srv_heap);
	exit_if_failed(hr);

	renderer->srv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hr = ID3D12Device_CreateCommandList(
		renderer->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer->command_allocator, renderer->pipeline_state_scene, &IID_ID3D12GraphicsCommandList1,
		&renderer->command_list);
	exit_if_failed(hr);

	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	// Constant buffers must be 256-byte aligned size
	UINT cb_size = (sizeof(Sendai_ConstantBuffer) + 255) & ~255;
	const D3D12_RESOURCE_DESC cb_desc = CD3DX12_RESOURCE_DESC_BUFFER(cb_size, D3D12_RESOURCE_FLAG_NONE, 0);

	hr = ID3D12Device_CreateCommittedResource(
		renderer->device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &cb_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		&renderer->constant_buffer);
	exit_if_failed(hr);

	renderer->fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (renderer->fence_event == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		exit_if_failed(hr);
	}

	D3D12_DESCRIPTOR_HEAP_DESC rtv_desc_heap_desc = {
	  .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	  .NumDescriptors = 2,
	  .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	  .NodeMask = 0,
	};
	hr = ID3D12Device_CreateDescriptorHeap(renderer->device, &rtv_desc_heap_desc, &IID_ID3D12DescriptorHeap, &renderer->rtv_descriptor_heap);
	exit_if_failed(hr);

	renderer->rtv_desc_increment = ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {
	  .Width = renderer->width,
	  .Height = renderer->height,
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
	hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)renderer->command_queue, renderer->hwnd, &swap_chain_desc, NULL, NULL, &renderer->swap_chain);
	exit_if_failed(hr);

	hr = IDXGISwapChain1_GetBuffer(renderer->swap_chain, 0, &IID_ID3D12Resource, &renderer->rtv_buffers[0]);
	exit_if_failed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->swap_chain, 1, &IID_ID3D12Resource, &renderer->rtv_buffers[1]);
	exit_if_failed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->rtv_descriptor_heap, &descriptor_handle);
	ID3D12Device_CreateRenderTargetView(renderer->device, renderer->rtv_buffers[0], NULL, descriptor_handle);
	renderer->rtv_handles[0] = descriptor_handle;

	descriptor_handle.ptr += renderer->rtv_desc_increment;
	ID3D12Device_CreateRenderTargetView(renderer->device, renderer->rtv_buffers[1], NULL, descriptor_handle);
	renderer->rtv_handles[1] = descriptor_handle;

	IDXGIFactory2_Release(dxgi_factory);
}

void SendaiRenderer_vertices(ID3D12Device *device, Sendai_Mesh *const mesh)
{
	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	const D3D12_RESOURCE_DESC buffer_resource = CD3DX12_RESOURCE_DESC_BUFFER(sizeof(Sendai_Vertex) * 24, D3D12_RESOURCE_FLAG_NONE, 0);

	// Note: using upload heaps to transfer static data like vert buffers is not
	// recommended. Every time the GPU needs it, the upload heap will be marshalled
	// over. Please read up on Default Heap usage. An upload heap is used here for
	// code simplicity and because there are very few verts to actually transfer
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &buffer_resource, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		&mesh->vertex_buffer);
	exit_if_failed(hr);

	mesh->vertex_buffer_view.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(mesh->vertex_buffer);
	mesh->vertex_buffer_view.StrideInBytes = sizeof(Sendai_Vertex);
	mesh->vertex_buffer_view.SizeInBytes = sizeof(Sendai_Vertex) * 24;
}

void SendaiRenderer_indices(ID3D12Device *device, Sendai_Mesh *const mesh)
{
	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
	  .Type = D3D12_HEAP_TYPE_UPLOAD,
	  .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	  .CreationNodeMask = 1,
	  .VisibleNodeMask = 1,
	};
	D3D12_RESOURCE_DESC ib_desc = CD3DX12_RESOURCE_DESC_BUFFER(mesh->index_count * sizeof(uint16_t), D3D12_RESOURCE_FLAG_NONE, 0);
	HRESULT hr = ID3D12Device_CreateCommittedResource(
		device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &ib_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
		(void **)(&mesh->index_buffer));
	exit_if_failed(hr);

	update_resource(mesh->index_buffer, mesh->indices, mesh->index_count * sizeof(uint16_t));

	mesh->index_buffer_view.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(mesh->index_buffer);
	mesh->index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
	mesh->index_buffer_view.SizeInBytes = (UINT)(mesh->index_count * sizeof(uint16_t));
}

void SendaiRenderer_update(Sendai_WorldRenderer *const renderer, Sendai_Camera *const camera, Sendai_Scene *scene)
{
	for (int i = 0; i < scene->mesh_count; ++i) {
		update_resource(scene->meshes[i].vertex_buffer, scene->meshes[i].vertices, scene->meshes[i].vertex_count * sizeof(Sendai_Vertex));
	}

	XMMATRIX view = Sendai_camera_view_matrix(camera->position, camera->look_direction, camera->up_direction);
	XMMATRIX proj = Sendai_camera_projection_matrix(XM_PIDIV4, renderer->aspect_ratio, 0.1f, 1000.0f);
	XMMATRIX mvp = XM_MAT_MULT(view, proj);
	mvp = XM_MAT_TRANSP(mvp);
	update_resource(renderer->constant_buffer, &mvp, sizeof(XMMATRIX));
}

void SendaiRenderer_draw(Sendai_WorldRenderer *const renderer, Sendai_Scene *scene)
{
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(renderer->command_list, scene->root_sign);
	ID3D12GraphicsCommandList_RSSetViewports(renderer->command_list, 1, &renderer->viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(renderer->command_list, 1, &renderer->scissor_rect);

	D3D12_RESOURCE_BARRIER resource_barrier = {
	  .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	  .Transition.pResource = renderer->rtv_buffers[renderer->rtv_index],
	  .Transition.Subresource = 0,
	  .Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
	  .Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
	  .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
	};
	const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->command_list, 1, &resource_barrier);
	ID3D12GraphicsCommandList_ClearRenderTargetView(renderer->command_list, renderer->rtv_handles[renderer->rtv_index], clearColor, 0, NULL);
	ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->command_list, 1, &renderer->rtv_handles[renderer->rtv_index], FALSE, NULL);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(renderer->command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(renderer->command_list, 0, ID3D12Resource_GetGPUVirtualAddress(renderer->constant_buffer));
	ID3D12DescriptorHeap *heaps[] = {renderer->srv_heap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(renderer->command_list, 1, heaps);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(renderer->command_list, 1, renderer->model_gpu_srv);
	for (int i = 0; i < scene->mesh_count; ++i) {
		ID3D12GraphicsCommandList_IASetVertexBuffers(renderer->command_list, 0, 1, &scene->meshes[i].vertex_buffer_view);
		ID3D12GraphicsCommandList_IASetIndexBuffer(renderer->command_list, &scene->meshes[i].index_buffer_view);
		ID3D12GraphicsCommandList_DrawIndexedInstanced(renderer->command_list, scene->meshes[i].index_count, 1, 0, 0, 0);
	}
	SendaiGui_draw(renderer->command_list);

	// Bring the rtv resource back to present state
	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Transition.pResource = renderer->rtv_buffers[renderer->rtv_index];
	resource_barrier.Transition.Subresource = 0;
	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->command_list, 1, &resource_barrier);

	SendaiRenderer_execute_commands(renderer);

	HRESULT hr = IDXGISwapChain2_Present(renderer->swap_chain, 1, 0);
	renderer->rtv_index = (renderer->rtv_index + 1) % FRAME_COUNT;
	if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
		MessageBox(NULL, L"D3D12 device is lost or removed!", L"Error", 0);
		return;
	}
	exit_if_failed(hr);
}

void SendaiRenderer_execute_commands(Sendai_WorldRenderer *const renderer)
{
	ID3D12GraphicsCommandList_Close(renderer->command_list);
	ID3D12CommandList *cmd_lists[] = {(ID3D12CommandList *)renderer->command_list};
	ID3D12CommandQueue_ExecuteCommandLists(renderer->command_queue, 1, cmd_lists);
	signal_and_wait(renderer);
	ID3D12CommandAllocator_Reset(renderer->command_allocator);
	ID3D12GraphicsCommandList_Reset(renderer->command_list, renderer->command_allocator, renderer->pipeline_state_scene);
}

void SendaiRenderer_destroy(Sendai_WorldRenderer *renderer)
{
	SendaiGui_destroy();
	for (int i = 0; i < FRAME_COUNT; ++i) {
		signal_and_wait(renderer);
		ID3D12Resource_Release(renderer->rtv_buffers[i]);
	}
	ID3D12DescriptorHeap_Release(renderer->rtv_descriptor_heap);
	IDXGISwapChain1_Release(renderer->swap_chain);
	ID3D12GraphicsCommandList_Release(renderer->command_list);
	ID3D12CommandAllocator_Release(renderer->command_allocator);
	ID3D12CommandQueue_Release(renderer->command_queue);
	ID3D12Fence_Release(renderer->fence);
	ID3D12PipelineState_Release(renderer->pipeline_state_scene);
	ID3D12Resource_Release(renderer->constant_buffer);
	ID3D12Device_Release(renderer->device);
	ID3D12Device_Release(renderer->srv_heap);
	ID3D12Device_Release(renderer->model_gpu_texture);
	CloseHandle(renderer->fence_event);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (void **)&debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}

void SendaiRenderer_swapchain_resize(Sendai_WorldRenderer *const renderer, int width, int height)
{
	for (int i = 0; i < FRAME_COUNT; ++i) {
		signal_and_wait(renderer);
		ID3D12Resource_Release(renderer->rtv_buffers[i]);
	}

	HRESULT hr = IDXGISwapChain1_ResizeBuffers(renderer->swap_chain, 2, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	exit_if_failed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->swap_chain, 0, &IID_ID3D12Resource, &renderer->rtv_buffers[0]);
	exit_if_failed(hr);
	hr = IDXGISwapChain1_GetBuffer(renderer->swap_chain, 1, &IID_ID3D12Resource, &renderer->rtv_buffers[1]);
	exit_if_failed(hr);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->rtv_descriptor_heap, &descriptor_handle);
	ID3D12Device_CreateRenderTargetView(renderer->device, renderer->rtv_buffers[0], NULL, descriptor_handle);
	renderer->rtv_handles[0] = descriptor_handle;
	descriptor_handle.ptr += renderer->rtv_desc_increment;
	ID3D12Device_CreateRenderTargetView(renderer->device, renderer->rtv_buffers[1], NULL, descriptor_handle);
	renderer->rtv_handles[1] = descriptor_handle;
	renderer->rtv_index = 0;
}

/****************************************************
	Implementation of private functions
*****************************************************/

static void signal_and_wait(Sendai_WorldRenderer *const renderer)
{
	HRESULT hr = ID3D12CommandQueue_Signal(renderer->command_queue, renderer->fence, ++renderer->fence_value);
	exit_if_failed(hr);
	ID3D12Fence_SetEventOnCompletion(renderer->fence, renderer->fence_value, renderer->fence_event);
	WaitForSingleObject(renderer->fence_event, INFINITE);
}

void update_resource(ID3D12Resource *resource, void *data, size_t data_size)
{
	UINT8 *begin = NULL;
	const D3D12_RANGE read_range = {0, 0};
	HRESULT hr = ID3D12Resource_Map(resource, 0, &read_range, (void **)&begin);
	exit_if_failed(hr);
	memcpy(begin, data, data_size);
	ID3D12Resource_Unmap(resource, 0, NULL);
}
