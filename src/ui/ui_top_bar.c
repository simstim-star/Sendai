#include "../core/pch.h"

#include "ui.h"
#include "ui_top_bar.h"

static const INT LAYOUT_ROW_COLS = 10;
static const FLOAT BAR_HEIGHT_PERCENTAGE = 0.05f;
static const FLOAT BUTTON_LAOYUT_PERCENTAGE = 0.8f;

void InfoPopup(UI_Renderer *UI, UI_TopBarState *State);

UI_EAction
UI_DrawTopBar(UI_Renderer *UI, UI_TopBarState *State)
{
	const float BarHeight = UI->Height * BAR_HEIGHT_PERCENTAGE;
	UI_EAction Action = UI_ACTION_NONE;
	if (nk_begin(UI->Context, "TopBar", nk_rect(0, 0, UI->Width, BarHeight), NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(UI->Context, BarHeight * BUTTON_LAOYUT_PERCENTAGE, LAYOUT_ROW_COLS);
		if (nk_button_label_styled(UI->Context, &UI->Context->style.contextual_button, "File")) {
			Action = UI_ACTION_FILE_OPEN;
		}
		if (nk_button_label_styled(UI->Context, &UI->Context->style.contextual_button, "Info")) {
			State->ShowInfo = !State->ShowInfo;
			Action = UI_ACTION_NONE;
		}
	}
	nk_end(UI->Context);

	if (State->ShowInfo) {
		InfoPopup(UI, State);
	}

	return Action;
}

void
InfoPopup(UI_Renderer *UI, UI_TopBarState *State)
{
	if (!State->ShowInfo) {
		return;
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO MemInfo;
	IDXGIAdapter3_QueryVideoMemoryInfo(State->Adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &MemInfo);

	struct nk_rect InfoRect = nk_rect(UI->Width * 0.5f - 225, UI->Height * 0.5f - 150, 450, 300);

	if (nk_begin(UI->Context, "Renderer Info", InfoRect, NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE)) {
		nk_layout_row_dynamic(UI->Context, 20, 1);
		nk_label(UI->Context, "HARDWARE", NK_TEXT_LEFT);
		DXGI_ADAPTER_DESC1 Desc = {0};
		IDXGIAdapter3_GetDesc1(State->Adapter, &Desc);

		const char *Vendor = (Desc.VendorId == 0x10DE)	 ? "NVIDIA"
							 : (Desc.VendorId == 0x1002) ? "AMD"
							 : (Desc.VendorId == 0x8086) ? "Intel"
														 : "Unknown";

		nk_labelf(UI->Context, NK_TEXT_LEFT, "  GPU: %ls (%s)", Desc.Description, Vendor);
		nk_labelf(UI->Context, NK_TEXT_LEFT, "  Device ID: 0x%X", Desc.DeviceId);

		nk_layout_row_dynamic(UI->Context, 2, 1);
		nk_rule_horizontal(UI->Context, nk_rgb(100, 100, 100), 0);

		nk_layout_row_dynamic(UI->Context, 20, 1);
		nk_label(UI->Context, "VRAM USAGE", NK_TEXT_LEFT);

		const nk_size UsageMB = MemInfo.CurrentUsage / (1024 * 1024);
		const nk_size BudgetMB = MemInfo.Budget / (1024 * 1024);
		const nk_size TotalMB = Desc.DedicatedVideoMemory / (1024 * 1024);

		nk_layout_row_dynamic(UI->Context, 25, 1);
		nk_progress(UI->Context, &UsageMB, BudgetMB, NK_FIXED);

		nk_layout_row_dynamic(UI->Context, 20, 1);
		nk_labelf(UI->Context, NK_TEXT_LEFT, "  Used: %llu MB / OS Budget: %llu MB", UsageMB, BudgetMB);
		nk_labelf(UI->Context, NK_TEXT_LEFT, "  Physical VRAM: %llu MB", TotalMB);

		const nk_size SharedMB = Desc.SharedSystemMemory / (1024 * 1024);
		nk_labelf(UI->Context, NK_TEXT_LEFT, "  Shared System Memory: %llu MB", SharedMB);

		nk_layout_row_dynamic(UI->Context, 30, 1);
		if (nk_button_label(UI->Context, "Close"))
			State->ShowInfo = false;
	} else {
		State->ShowInfo = false;
	}
	nk_end(UI->Context);
}
