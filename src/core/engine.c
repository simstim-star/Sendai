#include "pch.h"
#include "engine.h"
#include "../assets/gltf.h"
#include "../renderer/renderer.h"
#include "../ui/ui.h"
#include "../win32/file_dialog.h"

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void InitWindow(Sendai *Engine);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
static void EngineUpdate(Sendai *Engine);
static void EngineDraw(Sendai *Engine);
static void UI_Update(Sendai *Engine);
static void LoadPrimitivesIntoBuffers(R_World *Renderer, SendaiScene *Scene);

/****************************************************
	Public functions
*****************************************************/

int
Sendai_run()
{
	Sendai Engine = {.Title = L"Sendai",
					 .WorldRenderer = {.Width = 1280, .Height = 720},
					 .Camera = R_CameraSpawn((XMFLOAT3){0, 25, -100}),
					 .Scene =
						 {
						   .SceneArena = S_ArenaInit(GIGABYTES(2)),
						   .ModelsCount = 0,
						   .ModelsCapacity = 1000,
						 },
					 .bRunning = true};

	Engine.Scene.Models = S_ArenaAlloc(&Engine.Scene.SceneArena, sizeof(R_Model) * Engine.Scene.ModelsCapacity);

	Engine.Camera.Yaw = 2 * XM_PI;

	InitWindow(&Engine);
	R_Init(&Engine.WorldRenderer, Engine.hWnd);

	CreateSceneRootSig(Engine.WorldRenderer.Device, &Engine.Scene.RootSign);
	PCWSTR ShadersPath = wcscat(Engine.WorldRenderer.AssetsPath, L"/shaders/gltf/gltf.hlsl");
	CompileSceneVS(ShadersPath, &Engine.Scene.VS);
	CompileScenePS(ShadersPath, &Engine.Scene.PS);
	CreateScenePipelineState(&Engine.WorldRenderer, &Engine.Scene);

	UI_Init(&Engine.UI_Renderer, Engine.WorldRenderer.Width, Engine.WorldRenderer.Height, Engine.WorldRenderer.Device, Engine.WorldRenderer.CommandList);
	S_TimerInit(&Engine.Timer);

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		UI_InputBegin(&Engine.UI_Renderer);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UI_InputEnd(&Engine.UI_Renderer);
		if (Engine.bRunning) {
			EngineUpdate(&Engine);
			EngineDraw(&Engine);
		}
	}

	R_Destroy(&Engine.WorldRenderer);
	S_ArenaRelease(&Engine.Scene.SceneArena);
	return (int)(msg.wParam);
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
	RECT rect = {0, 0, (LONG)(engine->WorldRenderer.Width), (LONG)(engine->WorldRenderer.Height)};
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
		if (Engine && Engine->WorldRenderer.SwapChain) {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			R_SwapchainResize(&Engine->WorldRenderer, width, height);
			UI_Resize(&Engine->UI_Renderer, width, height);
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
	UI_Update(Engine);
}

static void
EngineDraw(Sendai *Engine)
{
	R_Update(&Engine->WorldRenderer, &Engine->Camera, &Engine->Scene);
	R_Draw(&Engine->WorldRenderer, &Engine->Scene);
}

void
UI_Update(Sendai *Engine)
{
	Engine->UI.BottomBar.FPS = Engine->Timer.FramesPerSecond;
	Engine->UI.BottomBar.FrameCounter = Engine->FrameCounter;
	UI_Action Action = UI_DrawTopBar(&Engine->UI_Renderer, &Engine->UI.TopBar) | UI_DrawBottomBar(&Engine->UI_Renderer, &Engine->UI.BottomBar);

	switch (Action) {
	case UI_ACTION_FILE_OPEN: {
		PWSTR FilePath = Win32SelectGLTFPath();
		if (FilePath == NULL) {
			break;
		}
		SendaiGLTF_LoadModel(FilePath, &Engine->Scene);
		LoadPrimitivesIntoBuffers(&Engine->WorldRenderer, &Engine->Scene);
		CoTaskMemFree(FilePath);
		break;
	}
	default:
		break;
	}
}

void
LoadPrimitivesIntoBuffers(R_World *Renderer, SendaiScene *Scene)
{
	void *pData;
	ID3D12Resource_Map(Renderer->UploadBuffer, 0, NULL, &pData);
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
				Primitive->VertexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->VertexBuffer) + CurrentVertexBufferOffset;
				Primitive->VertexBufferView.SizeInBytes = VertexBufferSize;
				Primitive->VertexBufferView.StrideInBytes = sizeof(R_Vertex);
				ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->VertexBuffer, CurrentVertexBufferOffset, Renderer->UploadBuffer, CurrentUploadBufferOffset, VertexBufferSize);
				CurrentVertexBufferOffset += VertexBufferSize;
				CurrentUploadBufferOffset += VertexBufferSize;

				UINT IndexBufferSize = Primitive->IndexCount * sizeof(UINT16);
				memcpy((BYTE *)pData + CurrentUploadBufferOffset, Primitive->Indices, IndexBufferSize);
				Primitive->IndexBufferView.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(Renderer->IndexBuffer) + CurrentIndexBufferOffset;
				Primitive->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
				Primitive->IndexBufferView.SizeInBytes = IndexBufferSize;
				ID3D12GraphicsCommandList_CopyBufferRegion(Renderer->CommandList, Renderer->IndexBuffer, CurrentIndexBufferOffset,
														   Renderer->UploadBuffer, CurrentUploadBufferOffset, IndexBufferSize);
				CurrentIndexBufferOffset += IndexBufferSize;
				CurrentUploadBufferOffset += IndexBufferSize;

				if (Primitive->AlbedoIndex >= 0) {
					UINT BaseSlot = Renderer->SrvCount;
					R_Texture *AlbedoTexture = &Scene->Models[ModelIdx].Images[Primitive->AlbedoIndex];
					Primitive->MaterialDescriptorBase = R_UploadTexture(Renderer, AlbedoTexture, BaseSlot);

					// dirty hack
					Renderer->SrvCount += 1;
				}
			}
		}
	}

	D3D12_RESOURCE_BARRIER BarrierVertexBuffer = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition.pResource = Renderer->VertexBuffer,
									  .Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
									  .Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
									  .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierVertexBuffer);

		D3D12_RESOURCE_BARRIER BarrierIndexBuffer = {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
									  .Transition.pResource = Renderer->IndexBuffer,
									  .Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
									  .Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER,
									  .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES};
	ID3D12GraphicsCommandList_ResourceBarrier(Renderer->CommandList, 1, &BarrierIndexBuffer);

	ID3D12Resource_Unmap(Renderer->UploadBuffer, 0, NULL);
}
