#include "../core/pch.h"

#include "ui_top_bar.h"
#include "ui.h"

static const INT LAYOUT_ROW_COLS = 10; 
static const FLOAT BAR_HEIGHT_PERCENTAGE = 0.05f;
// how much of the bar it will take
static const FLOAT BUTTON_LAOYUT_PERCENTAGE = 0.8f;

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
		if (nk_button_label_styled(UI->Context, &UI->Context->style.contextual_button, "Logs")) {
			State->ShowLog = !State->ShowLog;
			Action = UI_ACTION_NONE;
		}
	}
	nk_end(UI->Context);

	if (State->ShowLog) {
		UI_LogWindow(UI);
	}

	return Action;
}
