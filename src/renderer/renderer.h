#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "../assets/gltf.h"

#define FRAME_COUNT 2

typedef struct Sendai_Camera Sendai_Camera;

typedef struct Sendai_Renderer {
	HWND hwnd;
	UINT width;
	UINT height;
	float aspect_ratio;
	WCHAR assets_path[512];

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor_rect;

	DXGI_MODE_DESC fullscreen_mode;
	BOOL is_fullscreen;
	DXGI_MODE_DESC *display_modes;

	Sendai_Model model;

	IDXGISwapChain1 *swap_chain;
	ID3D12DescriptorHeap *rtv_descriptor_heap;
	ID3D12DescriptorHeap *srv_heap;
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
	UINT64 fence_value;
	HANDLE fence_event;
	ID3D12Fence *fence;

	/*****************************
		Resources
	*****************************/

	ID3D12Resource *vertex_buffer;
	ID3D12Resource *index_buffer;
	ID3D12Resource *constant_buffer;
	ID3D12Resource *model_gpu_texture;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;

	UINT srv_descriptor_size;
	D3D12_GPU_DESCRIPTOR_HANDLE model_gpu_srv;

	UINT8 *vb_mapped;
	UINT8 *cb_mapped;
} Sendai_Renderer;

void SendaiRenderer_init(Sendai_Renderer *const renderer, HWND hwnd);
void SendaiRenderer_indices(Sendai_Renderer *const renderer);
void SendaiRenderer_upload_texture(Sendai_Renderer *renderer, Sendai_Texture *source, ID3D12Resource **out_texture, D3D12_GPU_DESCRIPTOR_HANDLE *out_srv, UINT srv_index);
void SendaiRenderer_destroy(Sendai_Renderer *renderer);
void SendaiRenderer_update(Sendai_Renderer *const renderer, Sendai_Camera *const camera);
void SendaiRenderer_draw(Sendai_Renderer *const renderer);
void SendaiRenderer_execute_commands(Sendai_Renderer *const renderer);
void SendaiRenderer_swapchain_resize(Sendai_Renderer *const renderer, int width, int height);