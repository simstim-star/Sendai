#include "pch.h"

#include "../assets/gltf.h"
#include "../renderer/light.h"
#include "../renderer/renderer.h"
#include "../renderer/shader.h"
#include "../renderer/texture.h"
#include "../ui/ui.h"
#include "../win32/file_dialog.h"
#include "../win32/win_path.h"
#include "engine.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void InitWindow(Sendai *const Engine);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
static void EngineUpdate(Sendai *const Engine);
static void LoadPrimitivesIntoBuffers(R_Core *const Renderer, const R_Model *const Model);

/****************************************************
	Public functions
*****************************************************/

INT
S_Run(void)
{
	Sendai Engine = {.Title = L"Sendai",
					 .RendererCore = {.Width = 1280, .Height = 720},
					 .Camera = R_CameraSpawn((XMFLOAT3){0, 10, -20}),
					 .Scene =
						 {
						   .SceneArena = M_ArenaInit(MEGABYTES(512)),
						   .ModelsCount = 0,
						   .ModelsCapacity = 10000,
						 },
					 .bRunning = TRUE};

	Engine.Scene.Models = M_ArenaAlloc(&Engine.Scene.SceneArena, sizeof(R_Model) * Engine.Scene.ModelsCapacity);

	InitWindow(&Engine);
	R_Init(&Engine.RendererCore, Engine.hWnd);
	UI_Init(&Engine.UI, &Engine.RendererCore);
	R_LightsInit(&Engine.Scene, &Engine.Camera);
	S_TimerInit(&Engine.Timer);

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		UI_InputBegin(&Engine.UI.Renderer);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UI_InputEnd(&Engine.UI.Renderer);

		if (Engine.bRunning) {
			EngineUpdate(&Engine);
			R_Draw(&Engine.RendererCore, &Engine.Scene, &Engine.Camera);
		}
	}

	R_Destroy(&Engine.RendererCore);
	M_ArenaRelease(&Engine.Scene.SceneArena);

#if defined(_DEBUG)
	IDXGIDebug1 *debugDev = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, &debugDev))) {
		IDXGIDebug_ReportLiveObjects(debugDev, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
#endif
	return (INT)(msg.wParam);
}

void
S_DoNothing(Sendai *const Engine)
{
}

void
S_FileOpen(Sendai *const Engine)
{
	PWSTR FilePath = Win32SelectGLTFPath();
	if (FilePath == NULL) {
		return;
	}

	S_Scene *Scene = &Engine->Scene;
	if (Scene->UploadArena.Base) {
		M_ArenaRelease(&Scene->UploadArena);
	}
	
	Scene->UploadArena = M_ArenaInit(MEGABYTES(512));
	
	SendaiGLTF_LoadModel(FilePath, Scene);
	UINT ModelIdx = Scene->ModelsCount - 1;
	LoadPrimitivesIntoBuffers(&Engine->RendererCore, &Scene->Models[ModelIdx]);
	
	CoTaskMemFree(FilePath);
	M_ArenaRelease(&Scene->UploadArena);
}

void
S_WireframeMode(Sendai *const Engine)
{
	if (Engine->RendererCore.State != ERS_WIREFRAME) {
		Engine->RendererCore.State = ERS_WIREFRAME;
	} else {
		Engine->RendererCore.State = ERS_GLTF;
	}
}

void
S_GridMode(Sendai *const Engine)
{
	Engine->RendererCore.bDrawGrid = !Engine->RendererCore.bDrawGrid;
}

/****************************************************
	Implementation of private functions
*****************************************************/

void
InitWindow(Sendai *const Engine)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = Engine->hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
	wc.lpszClassName = L"SendaiClass";
	RegisterClassEx(&wc);
	RECT rect = {0, 0, (LONG)(Engine->RendererCore.Width), (LONG)(Engine->RendererCore.Height)};
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	Engine->hWnd = CreateWindow(wc.lpszClassName, Engine->Title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
								rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, Engine->hInstance, Engine);
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
		if (wParam == SIZE_MINIMIZED) {
			return 0;
		}
		if (Engine && Engine->RendererCore.SwapChain) {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			R_SwapchainResize(&Engine->RendererCore, width, height);
			UI_Resize(&Engine->UI.Renderer, width, height);
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
EngineUpdate(Sendai *const Engine)
{
	if (Engine->FrameCounter == 50) {
		Engine->FrameCounter = 0;
	}
	Engine->FrameCounter++;

	S_Tick(&Engine->Timer);
	R_CameraUpdate(&Engine->Camera, TicksToSeconds_FLOAT(Engine->Timer.ElapsedTicks));

	Engine->UI.State.BottomBar.FPS = Engine->Timer.FramesPerSecond;
	Engine->UI.State.BottomBar.FrameCounter = Engine->FrameCounter;
	Engine->UI.State.BottomBar.Scene = &Engine->Scene;
	Engine->UI.State.ToolBar.Camera = &Engine->Camera;
	void (*Action)(Sendai *const Engine) = UI_GetAction(&Engine->UI);
	Action(Engine);
}

void
LoadPrimitivesIntoBuffers(R_Core *const Renderer, const R_Model *const Model)
{
	for (int MeshIdx = 0; MeshIdx < Model->MeshesCount; ++MeshIdx) {
		const R_Mesh *const Mesh = &Model->Meshes[MeshIdx];
		for (int PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
			R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];
			UINT VertexBufferSize = sizeof(R_Vertex) * Primitive->VertexCount;
			memcpy(Renderer->VertexBufferUploadCpuAddress + Renderer->CurrentUploadBufferOffset, Primitive->Vertices, VertexBufferSize);
			Primitive->VertexBufferView.BufferLocation =
				ID3D12Resource_GetGPUVirtualAddress(Renderer->VertexBufferDefault) + Renderer->CurrentVertexBufferOffset;
			Primitive->VertexBufferView.SizeInBytes = VertexBufferSize;
			Primitive->VertexBufferView.StrideInBytes = sizeof(R_Vertex);
			ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->VertexBufferDefault, Renderer->CurrentVertexBufferOffset,
													   Renderer->VertexBufferUpload, Renderer->CurrentUploadBufferOffset, VertexBufferSize);
			Renderer->CurrentVertexBufferOffset += VertexBufferSize;
			Renderer->CurrentUploadBufferOffset += VertexBufferSize;

			UINT IndexBufferSize = Primitive->IndexCount * sizeof(UINT16);
			memcpy(Renderer->VertexBufferUploadCpuAddress + Renderer->CurrentUploadBufferOffset, Primitive->Indices, IndexBufferSize);
			Primitive->IndexBufferView.BufferLocation =
				ID3D12Resource_GetGPUVirtualAddress(Renderer->IndexBufferDefault) + Renderer->CurrentIndexBufferOffset;
			Primitive->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
			Primitive->IndexBufferView.SizeInBytes = IndexBufferSize;
			ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->IndexBufferDefault, Renderer->CurrentIndexBufferOffset,
													   Renderer->VertexBufferUpload, Renderer->CurrentUploadBufferOffset, IndexBufferSize);
			Renderer->CurrentIndexBufferOffset += IndexBufferSize;
			Renderer->CurrentUploadBufferOffset += IndexBufferSize;

			R_LoadPBRTextures(Primitive, Renderer);
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
}
