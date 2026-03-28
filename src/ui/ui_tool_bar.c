#include "../core/pch.h"

#include "ui.h"
#include "ui_bottom_panel.h"

#include "../core/camera.h"

static const float HEIGHT_PERCENTAGE = 0.05f;
static const float BUTTON_SIZE = 50.0f;
static const float WINDOW_WIDTH = 650.0f;
static const int TAB_COLS = 8;

UI_EAction
UI_DrawToolbar(UI_Renderer *UI, UI_ToolBarState *State)
{
	struct nk_context *Ctx = UI->Context;
	UI_EAction Action = UI_ACTION_NONE;

	const float TopBarHeight = UI->Height * HEIGHT_PERCENTAGE;
	struct nk_rect WindowRect = nk_rect(10, TopBarHeight + 10, WINDOW_WIDTH, BUTTON_SIZE);

	nk_style_push_vec2(Ctx, &Ctx->style.window.padding, nk_vec2(5, 5));

	if (nk_begin(Ctx, "Toolbar", WindowRect, NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_begin(Ctx, NK_STATIC, BUTTON_SIZE - 10, TAB_COLS);
		{
			nk_layout_row_push(Ctx, BUTTON_SIZE - 10);
			if (nk_button_image(Ctx, UI_TEXTURES[UI_EUT_WIREFRAME])) {
				Action = UI_ACTION_WIREFRAME_BUTTON_CLICKED;
				State->Wireframe = !State->Wireframe;
			}

			nk_layout_row_push(Ctx, BUTTON_SIZE - 10);
			if (nk_button_image(Ctx, UI_TEXTURES[UI_EUT_GRID])) {
				Action = UI_ACTION_GRID_BUTTON_CLICKED;
				State->Grid = !State->Grid;
			}

			nk_layout_row_push(Ctx, 10);
			nk_spacing(Ctx, 1);

			nk_layout_row_push(Ctx, BUTTON_SIZE - 10);
			{
				struct nk_style_button static_btn = Ctx->style.button;
				static_btn.hover = static_btn.normal;
				static_btn.active = static_btn.normal;
				nk_button_image_styled(Ctx, &static_btn, UI_TEXTURES[UI_EUT_CAMERA]);
			}

			nk_layout_row_push(Ctx, 110);
			nk_property_float(Ctx, "#X:", -1000.0f, &State->Camera->Position.x, 1000.0f, 0.1f, 0.05f);
			nk_layout_row_push(Ctx, 110);
			nk_property_float(Ctx, "#Y:", -1000.0f, &State->Camera->Position.y, 1000.0f, 0.1f, 0.05f);
			nk_layout_row_push(Ctx, 110);
			nk_property_float(Ctx, "#Z:", -1000.0f, &State->Camera->Position.z, 1000.0f, 0.1f, 0.05f);
			nk_layout_row_push(Ctx, 110);
			nk_property_float(Ctx, "Speed:", 0.0f, &State->Camera->MoveSpeed, 100.0f, 1.0f, 0.5f);
		}
		nk_layout_row_end(Ctx);
	}
	nk_end(Ctx);
	nk_style_pop_vec2(Ctx);

	return Action;
}