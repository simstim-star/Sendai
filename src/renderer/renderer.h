#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#define FRAME_COUNT 2

typedef struct SR_Renderer {
	UINT width;
	UINT height;
	float aspect_ratio;
	WCHAR assets_path[512];

	void *data;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor_rect;

	IDXGISwapChain1 *swap_chain;
	ID3D12DescriptorHeap *rtv_descriptor_heap;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[FRAME_COUNT];
	ID3D12Resource *rtv_buffers[FRAME_COUNT];
	UINT rtv_desc_increment;
	UINT rtv_index;

	ID3D12Device *device;
	ID3D12CommandQueue *command_queue;

	ID3D12CommandAllocator *command_allocator;
	ID3D12GraphicsCommandList *command_list;

	ID3D12RootSignature *root_sig;
	ID3D12PipelineState *pipeline_state;

	/*****************************
		Synchronization objects
	*****************************/

	UINT frame_index;
	ID3D12Fence *fence;
	UINT64 fence_value;
	HANDLE fence_event;

	/*****************************
		Resources
	*****************************/

	ID3D12Resource *vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
} SR_Renderer;

void SR_init(SR_Renderer *const renderer);
void SR_destroy(SR_Renderer *renderer);
void SR_update(SR_Renderer *const renderer);
void SR_draw(SR_Renderer *const renderer);
void SR_execute_commands(SR_Renderer *const renderer);
void SR_swapchain_resize(SR_Renderer *const renderer, int width, int height);