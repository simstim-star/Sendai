#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include "../gui/gui.h"
#include "../dx_helpers/desc_helpers.h"
#include "../error/error.h"
#include "../win32/win_path.h"
#include "../win32/window.h"
#include "renderer.h"

#include "dxgidebug.h"
#include "render_types.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void signal_and_wait(SR_Renderer *const renderer);
static void update_data(SR_Renderer *const renderer, size_t data_size);

/****************************************************
	Public functions
*****************************************************/

void SR_init(SR_Renderer *const renderer) {
	renderer->aspect_ratio = (float)(renderer->width) / (float)(renderer->height);
	renderer->viewport = (D3D12_VIEWPORT){0.0f, 0.0f, (float)(renderer->width), (float)(renderer->height)};
	renderer->scissor_rect = (D3D12_RECT){0, 0, (LONG)(renderer->width), (LONG)(renderer->height)};
	win32_curr_path(renderer->assets_path, _countof(renderer->assets_path));

	/* D3D12 setup */

	int isDebugFactory = 0;
#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	ID3D12Debug1 *debugController = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void **)&debugController))) {
		ID3D12Debug_EnableDebugLayer(debugController);
		isDebugFactory |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1_Release(debugController);
	}
#endif
	IDXGIFactory2 *dxgi_factory = NULL;
	HRESULT hr = CreateDXGIFactory2(isDebugFactory, &IID_IDXGIFactory2, (void **)&dxgi_factory);
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

	const D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {.NumParameters = 0,
														   .pParameters = NULL,
														   .NumStaticSamplers = 0,
														   .pStaticSamplers = NULL,
														   .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

	ID3DBlob *signature = NULL;
	ID3DBlob *error = NULL;
	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	exit_if_failed(hr);

	const LPVOID buffer_ptr = ID3D10Blob_GetBufferPointer(signature);
	const SIZE_T buffer_size = ID3D10Blob_GetBufferSize(signature);
	hr = ID3D12Device_CreateRootSignature(renderer->device, 0, buffer_ptr, buffer_size, &IID_ID3D12RootSignature, &renderer->root_sig);
	exit_if_failed(hr);

	ID3DBlob *vertex_shader = NULL;
	ID3DBlob *pixel_shader = NULL;
#if defined(_DEBUG)
	const UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	const UINT compile_flags = 0;
#endif
	const wchar_t *shaders_path = wcscat(renderer->assets_path, L"src/shaders/triangle/triangle.hlsl");
	hr = D3DCompileFromFile(shaders_path, NULL, NULL, "VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, NULL);
	exit_if_failed(hr);
	hr = D3DCompileFromFile(shaders_path, NULL, NULL, "PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, NULL);
	exit_if_failed(hr);

	const D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {{.SemanticName = "POSITION",
															 .SemanticIndex = 0,
															 .Format = DXGI_FORMAT_R32G32B32_FLOAT,
															 .InputSlot = 0,
															 .AlignedByteOffset = 0,
															 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
															 .InstanceDataStepRate = 0},
															{.SemanticName = "COLOR",
															 .SemanticIndex = 0,
															 .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
															 .InputSlot = 0,
															 .AlignedByteOffset = 12,
															 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
															 .InstanceDataStepRate = 0}};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		.pRootSignature = renderer->root_sig,
		.InputLayout = (D3D12_INPUT_LAYOUT_DESC){.pInputElementDescs = input_element_descs, .NumElements = _countof(input_element_descs)},
		.VS =
			(D3D12_SHADER_BYTECODE){
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(vertex_shader),
				.BytecodeLength = ID3D10Blob_GetBufferSize(vertex_shader),
			},
		.PS =
			(D3D12_SHADER_BYTECODE){
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(pixel_shader),
				.BytecodeLength = ID3D10Blob_GetBufferSize(pixel_shader),
			},
		.RasterizerState = CD3DX12_DEFAULT_RASTERIZER_DESC(),
		.BlendState = CD3DX12_DEFAULT_BLEND_DESC(),
		.DepthStencilState.DepthEnable = FALSE,
		.DepthStencilState.StencilEnable = FALSE,
		.SampleMask = UINT_MAX,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc.Count = 1,
	};

	hr = ID3D12Device_CreateGraphicsPipelineState(renderer->device, &pso_desc, &IID_ID3D12PipelineState, &renderer->pipeline_state);
	exit_if_failed(hr);

	hr = ID3D12Device_CreateCommandList(renderer->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer->command_allocator, renderer->pipeline_state,
										&IID_ID3D12GraphicsCommandList1, &renderer->command_list);
	exit_if_failed(hr);

	SR_Vertex *verts = malloc(3 * sizeof(SR_Vertex));
	// Coordinates are in relation to the screen center, left-handed (+z to screen inside, +y up, +x right)
	verts[0] = (SR_Vertex){{0.0f, 0.25f * renderer->aspect_ratio, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
	verts[1] = (SR_Vertex){{0.25f, -0.25f * renderer->aspect_ratio, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
	verts[2] = (SR_Vertex){{-0.25f, -0.25f * renderer->aspect_ratio, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};

	renderer->data = verts;

	const D3D12_HEAP_PROPERTIES heap_property_upload = (D3D12_HEAP_PROPERTIES){
		.Type = D3D12_HEAP_TYPE_UPLOAD,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1,
		.VisibleNodeMask = 1,
	};
	const D3D12_RESOURCE_DESC buffer_resource = CD3DX12_RESOURCE_DESC_BUFFER(sizeof(SR_Vertex) * 3, D3D12_RESOURCE_FLAG_NONE, 0);

	// Note: using upload heaps to transfer static data like vert buffers is not
	// recommended. Every time the GPU needs it, the upload heap will be marshalled
	// over. Please read up on Default Heap usage. An upload heap is used here for
	// code simplicity and because there are very few verts to actually transfer
	hr = ID3D12Device_CreateCommittedResource(renderer->device, &heap_property_upload, D3D12_HEAP_FLAG_NONE, &buffer_resource,
											  D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &renderer->vertex_buffer);
	exit_if_failed(hr);

	renderer->vertex_buffer_view.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(renderer->vertex_buffer);
	renderer->vertex_buffer_view.StrideInBytes = sizeof(SR_Vertex);
	renderer->vertex_buffer_view.SizeInBytes = sizeof(SR_Vertex) * 3;

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
	hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)renderer->command_queue, G_HWND, &swap_chain_desc, NULL, NULL,
											  &renderer->swap_chain);
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

void SR_update(SR_Renderer *const renderer) {
	update_data(renderer, sizeof(SR_Vertex) * 3);
}

void SR_draw(SR_Renderer *const renderer) {
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(renderer->command_list, renderer->root_sig);
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
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->command_list, 1, &resource_barrier);
	const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
	ID3D12GraphicsCommandList_ClearRenderTargetView(renderer->command_list, renderer->rtv_handles[renderer->rtv_index], clearColor, 0, NULL);
	ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->command_list, 1, &renderer->rtv_handles[renderer->rtv_index], FALSE, NULL);
	ID3D12GraphicsCommandList_IASetVertexBuffers(renderer->command_list, 0, 1, &renderer->vertex_buffer_view);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(renderer->command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_DrawInstanced(renderer->command_list, 3, 1, 0, 0);

	SGUI_draw(renderer->command_list);

	// Bring the rtv resource back to present state
	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Transition.pResource = renderer->rtv_buffers[renderer->rtv_index];
	resource_barrier.Transition.Subresource = 0;
	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->command_list, 1, &resource_barrier);

	SR_execute_commands(renderer);

	HRESULT hr = IDXGISwapChain2_Present(renderer->swap_chain, 1, 0);
	renderer->rtv_index = (renderer->rtv_index + 1) % FRAME_COUNT;
	if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
		/* to recover from this, you'll need to recreate device and all the resources */
		MessageBoxW(NULL, L"D3D12 device is lost or removed!", L"Error", 0);
		return;
	} else if (hr == DXGI_STATUS_OCCLUDED) {
		/* window is not visible, so vsync won't work. Let's sleep a bit to reduce CPU usage */
		Sleep(10);
	}
	exit_if_failed(hr);
}

void SR_execute_commands(SR_Renderer *const renderer) {
	ID3D12GraphicsCommandList_Close(renderer->command_list);
	ID3D12CommandList *cmd_lists[] = {(ID3D12CommandList *)renderer->command_list};
	ID3D12CommandQueue_ExecuteCommandLists(renderer->command_queue, 1, cmd_lists);
	signal_and_wait(renderer);
	ID3D12CommandAllocator_Reset(renderer->command_allocator);
	ID3D12GraphicsCommandList_Reset(renderer->command_list, renderer->command_allocator, renderer->pipeline_state);
}

void SR_destroy(SR_Renderer *renderer) {
	SGUI_destroy();
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
	ID3D12RootSignature_Release(renderer->root_sig);
	ID3D12PipelineState_Release(renderer->pipeline_state);
	ID3D12Resource_Release(renderer->vertex_buffer);
	ID3D12Device_Release(renderer->device);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (void **)&debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
}

void SR_swapchain_resize(SR_Renderer *const renderer, int width, int height) {
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

static void signal_and_wait(SR_Renderer *const renderer) {
	HRESULT hr = ID3D12CommandQueue_Signal(renderer->command_queue, renderer->fence, ++renderer->fence_value);
	exit_if_failed(hr);

	while (ID3D12Fence_GetCompletedValue(renderer->fence) != renderer->fence_value) {
		SwitchToThread(); /* Allow windows to do other work */
	}
}

static void update_data(SR_Renderer *const renderer, size_t data_size) {
	UINT8 *vertex_data_begin = NULL;
	// We do not intend to read from this resource on the CPU, only write
	const D3D12_RANGE read_range = {0, 0};
	HRESULT hr = ID3D12Resource_Map(renderer->vertex_buffer, 0, &read_range, (void **)&vertex_data_begin);
	exit_if_failed(hr);
	memcpy(vertex_data_begin, renderer->data, data_size);
	ID3D12Resource_Unmap(renderer->vertex_buffer, 0, NULL);
}
