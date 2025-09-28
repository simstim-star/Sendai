#include "../core/engine.h"
#include "../renderer/renderer.h"
#include "window.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	return win32_run(&(snd_engine_t){.title = "Sendai",
									 .renderer =
										 (snr_renderer_t){
											 .width = 1280,
											 .height = 720,
										 }},
					 hInstance, nCmdShow);
}