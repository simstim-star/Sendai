#include "pch.h"

#include "engine.h"
#include "../assets/gltf.h"
#include "../renderer/renderer.h"
#include "../renderer/shader.h"
#include "../ui/ui.h"
#include "../win32/file_dialog.h"
#include "../win32/win_path.h"

static const UINT8 BLACK_PIXEL[] = {0, 0, 0, 255};
static const UINT8 WHITE_PIXEL[] = {255, 255, 255, 255};

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void InitWindow(Sendai *Engine);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
static void EngineUpdate(Sendai *Engine);
static void EngineDraw(Sendai *Engine);
static void LoadPrimitivesIntoBuffers(R_Core *Renderer, S_Scene *Scene);

void LoadPBRTextures(R_Primitive *Primitive, R_Core *Renderer, S_Scene *Scene, int ModelIdx);
void R_BindTextureToSlot(R_Core *Renderer, S_Scene *Scene, int ModelIdx, int TextureIdx, UINT HeapSlot, UINT32 FallbackColor);

/****************************************************
	Public functions
*****************************************************/

INT
S_Run()
{
	Sendai Engine = {.Title = L"Sendai",
					 .RendererCore = {.Width = 1280, .Height = 720},
					 .Camera = R_CameraSpawn((XMFLOAT3){0, 0, 0}),
					 .Scene =
						 {
						   .SceneArena = S_ArenaInit(GIGABYTES(2)),
						   .ModelsCount = 0,
						   .ModelsCapacity = 10000,
						 },
					 .bRunning = TRUE};

	Engine.Scene.Models = S_ArenaAlloc(&Engine.Scene.SceneArena, sizeof(R_Model) * Engine.Scene.ModelsCapacity);
	Engine.Camera.Yaw = 2 * XM_PI;

	InitWindow(&Engine);
	R_Init(&Engine.RendererCore, Engine.hWnd);
	UI_Init(&Engine.RendererUI, &Engine.RendererCore);
	S_TimerInit(&Engine.Timer);

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		UI_InputBegin(&Engine.RendererUI);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UI_InputEnd(&Engine.RendererUI);

		if (Engine.bRunning) {
			EngineUpdate(&Engine);
			EngineDraw(&Engine);
		}
	}

	ID3D12RootSignature_Release(Engine.RendererCore.RootSignPBR);
	R_Destroy(&Engine.RendererCore);
	S_ArenaRelease(&Engine.Scene.SceneArena);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, &debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
	return (int)(msg.wParam);
}

void
S_FileOpen(Sendai *Engine)
{
	PWSTR FilePath = Win32SelectGLTFPath();
	if (FilePath == NULL) {
		return;
	}
	SendaiGLTF_LoadModel(FilePath, &Engine->Scene);
	LoadPrimitivesIntoBuffers(&Engine->RendererCore, &Engine->Scene);
	CoTaskMemFree(FilePath);
}

void
S_WireframeMode(Sendai *Engine)
{
	if (Engine->RendererCore.State != ERS_WIREFRAME) {
		Engine->RendererCore.State = ERS_WIREFRAME;
	} else {
		Engine->RendererCore.State = ERS_GLTF;
	}
}

/****************************************************
	Implementation of private functions
*****************************************************/

void
InitWindow(Sendai *engine)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = engine->hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
	wc.lpszClassName = L"SendaiClass";
	RegisterClassEx(&wc);
	RECT rect = {0, 0, (LONG)(engine->RendererCore.Width), (LONG)(engine->RendererCore.Height)};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	engine->hWnd = CreateWindow(wc.lpszClassName, engine->Title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
								rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, engine->hInstance, engine);
}

LRESULT CALLBACK
WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	Sendai *Engine = (Sendai *)(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (Message) {
	case WM_CREATE: {
		LPCREATESTRUCT pCreateStruct = (LPCREATESTRUCT)(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)(pCreateStruct->lpCreateParams));
		return 0;
	}
	case WM_SIZE:
		if (Engine && Engine->RendererCore.SwapChain) {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			R_SwapchainResize(&Engine->RendererCore, width, height);
			UI_Resize(&Engine->RendererUI, width, height);
		}
		return 0;

	case WM_PAINT:
		ValidateRect(hWnd, NULL);
		return 0;

	case WM_KEYDOWN:
		if (Engine) {
			R_CameraOnKeyDown(&Engine->Camera, wParam);
		}
		return 0;

	case WM_KEYUP:
		if (Engine) {
			R_CameraOnKeyUp(&Engine->Camera, wParam);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	if (UI_HandleEvent(hWnd, Message, wParam, lParam))
		return 0;

	return DefWindowProc(hWnd, Message, wParam, lParam);
}

static void
EngineUpdate(Sendai *Engine)
{
	if (Engine->FrameCounter == 50) {
		Engine->FrameCounter = 0;
	}
	Engine->FrameCounter++;

	S_Tick(&Engine->Timer);
	R_CameraUpdate(&Engine->Camera, TicksToSeconds_FLOAT(Engine->Timer.ElapsedTicks));
	Engine->Scene.Data =
		(R_SceneData){.CameraPosition = Engine->Camera.Position, .LightPosition = {10.0f, 1.0f, 1.0f}, .LightColor = {100.0f, 1.0f, 1.0f}};
	UI_Update(Engine);
}

static void
EngineDraw(Sendai *Engine)
{
	R_Draw(&Engine->RendererCore, &Engine->Scene, &Engine->Camera);
}

void
LoadPrimitivesIntoBuffers(R_Core *Renderer, S_Scene *Scene)
{
	void *pData;
	ID3D12Resource_Map(Renderer->VertexBufferUpload, 0, NULL, &pData);
	UINT64 CurrentUploadBufferOffset = 0;
	UINT64 CurrentVertexBufferOffset = 0;
	UINT64 CurrentIndexBufferOffset = 0;

	for (int ModelIdx = 0; ModelIdx < Scene->ModelsCount; ++ModelIdx) {
		for (int MeshIdx = 0; MeshIdx < Scene->Models[ModelIdx].MeshesCount; ++MeshIdx) {
			R_Mesh *Mesh = &Scene->Models[ModelIdx].Meshes[MeshIdx];
			for (int PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
				R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];
				UINT VertexBufferSize = sizeof(R_Vertex) * Primitive->VertexCount;
				memcpy((BYTE *)pData + CurrentUploadBufferOffset, Primitive->Vertices, VertexBufferSize);
				Primitive->VertexBufferView.BufferLocation =
					ID3D12Resource_GetGPUVirtualAddress(Renderer->VertexBufferDefault) + CurrentVertexBufferOffset;
				Primitive->VertexBufferView.SizeInBytes = VertexBufferSize;
				Primitive->VertexBufferView.StrideInBytes = sizeof(R_Vertex);
				ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->VertexBufferDefault, CurrentVertexBufferOffset,
														   Renderer->VertexBufferUpload, CurrentUploadBufferOffset, VertexBufferSize);
				CurrentVertexBufferOffset += VertexBufferSize;
				CurrentUploadBufferOffset += VertexBufferSize;

				UINT IndexBufferSize = Primitive->IndexCount * sizeof(UINT16);
				memcpy((BYTE *)pData + CurrentUploadBufferOffset, Primitive->Indices, IndexBufferSize);
				Primitive->IndexBufferView.BufferLocation =
					ID3D12Resource_GetGPUVirtualAddress(Renderer->IndexBufferDefault) + CurrentIndexBufferOffset;
				Primitive->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
				Primitive->IndexBufferView.SizeInBytes = IndexBufferSize;
				ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->IndexBufferDefault, CurrentIndexBufferOffset,
														   Renderer->VertexBufferUpload, CurrentUploadBufferOffset, IndexBufferSize);
				CurrentIndexBufferOffset += IndexBufferSize;
				CurrentUploadBufferOffset += IndexBufferSize;

				LoadPBRTextures(Primitive, Renderer, Scene, ModelIdx);
			}
		}
	}

	D3D12_RESOURCE_BARRIER BarrierVertexBuffer = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
												  .Transition.pResource = Renderer->VertexBufferDefault,
												  .Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
												  .Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
												  .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierVertexBuffer);

	D3D12_RESOURCE_BARRIER BarrierIndexBuffer = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
												 .Transition.pResource = Renderer->IndexBufferDefault,
												 .Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
												 .Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER,
												 .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierIndexBuffer);

	ID3D12Resource_Unmap(Renderer->VertexBufferUpload, 0, NULL);
}

void
LoadPBRTextures(R_Primitive *Primitive, R_Core *Renderer, S_Scene *Scene, int ModelIdx)
{
	UINT BaseDescriptorIndex = Renderer->TexturesCount;

	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(Renderer->TexturesHeap, &Primitive->MaterialDescriptorBase);
	UINT DescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Renderer->Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	Primitive->MaterialDescriptorBase.ptr += (UINT64)BaseDescriptorIndex * DescriptorSize;

	// Albedo (t0)
	R_BindTextureToSlot(Renderer, Scene, ModelIdx, Primitive->AlbedoIndex, BaseDescriptorIndex + 0, BLACK_PIXEL);

	// Normal (t1)
	R_BindTextureToSlot(Renderer, Scene, ModelIdx, Primitive->NormalIndex, BaseDescriptorIndex + 1, WHITE_PIXEL);

	// Metallic (t2)
	R_BindTextureToSlot(Renderer, Scene, ModelIdx, Primitive->MetallicIndex, BaseDescriptorIndex + 2, BLACK_PIXEL);

	// Roughness (t3)
	R_BindTextureToSlot(Renderer, Scene, ModelIdx, Primitive->RoughnessIndex, BaseDescriptorIndex + 3, BLACK_PIXEL);

	// AO (t4)
	R_BindTextureToSlot(Renderer, Scene, ModelIdx, Primitive->OcclusionIndex, BaseDescriptorIndex + 4, BLACK_PIXEL);
}

void
R_BindTextureToSlot(R_Core *Renderer, S_Scene *Scene, int ModelIdx, int TextureIdx, UINT HeapSlot, UINT32 FallbackColor)
{
	R_Texture *Target;
	R_Texture Dummy = {0};
	char DummyName[64];
	if (TextureIdx >= 0) {
		Target = &Scene->Models[ModelIdx].Images[TextureIdx];
	} else {
		snprintf(DummyName, sizeof(DummyName), "Fallback_M%d_S%u", ModelIdx, HeapSlot);
		Dummy.Pixels = &FallbackColor;
		Dummy.Width = 1;
		Dummy.Height = 1;
		Dummy.Name = DummyName;
		Target = &Dummy;
	}
	R_UploadTexture(Renderer, Target, HeapSlot);
}