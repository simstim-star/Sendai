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
static void UIUpdate(Sendai *Engine);
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

	UI_Init(&Engine.UI, Engine.WorldRenderer.Width, Engine.WorldRenderer.Height, Engine.WorldRenderer.Device, Engine.WorldRenderer.CommandList);
	S_TimerInit(&Engine.Timer);

	ShowWindow(Engine.hWnd, SW_MAXIMIZE);

	MSG msg = {0};
	while (Engine.bRunning) {
		UI_InputBegin(&Engine.UI);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				Engine.bRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UI_InputEnd(&Engine.UI);
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
			UI_Resize(&Engine->UI, width, height);
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
	S_Tick(&Engine->Timer);
	R_CameraUpdate(&Engine->Camera, TicksToSeconds_FLOAT(Engine->Timer.ElapsedTicks));
	UIUpdate(Engine);
}

static void
EngineDraw(Sendai *Engine)
{
	R_Update(&Engine->WorldRenderer, &Engine->Camera, &Engine->Scene);
	R_Draw(&Engine->WorldRenderer, &Engine->Scene);
}

void
UIUpdate(Sendai *Engine)
{
	UI_Action Action = UI_DrawTopBar(&Engine->UI) | UI_DrawBottomBar(&Engine->UI);

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
	for (int ModelIdx = 0; ModelIdx < Scene->ModelsCount; ++ModelIdx) {
		for (int MeshIdx = 0; MeshIdx < Scene->Models[ModelIdx].MeshesCount; ++MeshIdx) {
			R_Mesh *Mesh = &Scene->Models[ModelIdx].Meshes[MeshIdx];
			for (int PrimitiveIdx = 0; PrimitiveIdx < Mesh->PrimitivesCount; ++PrimitiveIdx) {
				R_Primitive *Primitive = &Mesh->Primitives[PrimitiveIdx];

				UINT VertexBufferSize = sizeof(R_Vertex) * Primitive->VertexCount;
				Primitive->VertexBufferView =
					(D3D12_VERTEX_BUFFER_VIEW){.BufferLocation = R_UploadStaticData(Renderer->Device, Renderer->CommandList, VertexBufferSize,
																					Primitive->Vertices, &Primitive->VertexBuffer),
											   .SizeInBytes = VertexBufferSize,
											   .StrideInBytes = sizeof(R_Vertex)};

				UINT IndexBufferSize = Primitive->IndexCount * sizeof(UINT16);
				Primitive->IndexBufferView = (D3D12_INDEX_BUFFER_VIEW){
				  .BufferLocation =
					  R_UploadStaticData(Renderer->Device, Renderer->CommandList, IndexBufferSize, Primitive->Indices, &Primitive->IndexBuffer),
				  .Format = DXGI_FORMAT_R16_UINT,
				  .SizeInBytes = Primitive->IndexCount * sizeof(UINT16),
				};

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
}
