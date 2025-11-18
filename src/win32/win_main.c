#include "../core/engine.h"
#include "../renderer/renderer.h"
#include "window.h"

#include "DirectXMathC.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	SC_Camera camera = SC_camera_spawn((XMFLOAT3){0, 75, 150});
	camera.move_speed = 150.f;
	return win32_run(
		&(SC_Engine){
			.title = "Sendai",
			.renderer =
				(SR_Renderer){
					.width = 1280,
					.height = 720,
				},
			.camera = camera,
		},
		hInstance, nCmdShow);
}