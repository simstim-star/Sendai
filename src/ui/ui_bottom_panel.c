#include "../core/pch.h"

#include "ui_bottom_panel.h"
#include "ui.h"

#include "../core/log.h"
#include "../core/scene.h"
#include "../renderer/render_types.h"

static const float HANDLE_HEIGHT = 10.0f;
static const float INFO_BAR_HEIGHT = 22.0f;
static const float TAB_BAR_HEIGHT = 30.0f;
static const float MIN_PANEL_HEIGHT_PERCENTAGE = 0.08f;
static const float MAX_PANEL_HEIGHT_PERCENTAGE = 0.9f;
static const float TAB_WIDTH = 80.0f;

#define INFO_BAR_BACKGROUND_COLOR nk_rgba(30, 30, 35, 255)
#define INFO_BAR_TEXT_COLOR nk_rgba(0, 255, 0, 255)
#define INFO_BAR_PADDING nk_vec2(4, 2)

/****************************************************
	Forward declaration of private functions
*****************************************************/

static void BottomPanelDraggingArea(struct nk_context *Ctx, const float HandleHeight, UI_Renderer *UI, UI_BottomPanelState *State);
static void BottomPanelTabArea(struct nk_context *Ctx, const float TabBarHeight, UI_BottomPanelState *State);
static void BottomPanelContentArea(struct nk_context *Ctx, UI_BottomPanelState *State, const float HandleHeight, const float TabBarHeight);
static void BottomBarInfoArea(struct nk_context *Ctx, UI_Renderer *UI, const float InfoBarHeight, UI_BottomPanelState *State);

/****************************************************
	Public functions
*****************************************************/

UI_EAction
UI_DrawBottomPanel(UI_Renderer *UI, UI_BottomPanelState *State)
{
	struct nk_context *Ctx = UI->Context;
	State->BottomPanelHeight = fmax(State->BottomPanelHeight, UI->Height * MIN_PANEL_HEIGHT_PERCENTAGE);
	State->BottomPanelHeight = fmin(State->BottomPanelHeight, UI->Height * MAX_PANEL_HEIGHT_PERCENTAGE);

	float MaxAvailableHeight = UI->Height - INFO_BAR_HEIGHT;
	struct nk_rect BarRect = nk_rect(0, MaxAvailableHeight - State->BottomPanelHeight, UI->Width, State->BottomPanelHeight);

	if (nk_begin(Ctx, "BottomBar", BarRect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		BottomPanelDraggingArea(Ctx, HANDLE_HEIGHT, UI, State);
		BottomPanelTabArea(Ctx, TAB_BAR_HEIGHT, State);
		BottomPanelContentArea(Ctx, State, HANDLE_HEIGHT, TAB_BAR_HEIGHT);
	}
	nk_end(Ctx);

	BottomBarInfoArea(Ctx, UI, INFO_BAR_HEIGHT, State);

	nk_style_pop_color(Ctx);
	nk_style_pop_vec2(Ctx);
	nk_style_pop_style_item(Ctx);

	return UI_ACTION_NONE;
}

/****************************************************
	Implementation of private functions
*****************************************************/

void
BottomPanelDraggingArea(struct nk_context *Ctx, const float HandleHeight, UI_Renderer *UI, UI_BottomPanelState *State)
{
	nk_layout_row_static(Ctx, HandleHeight, UI->Width, 1);
	struct nk_rect Bounds = nk_widget_bounds(Ctx);
	struct nk_command_buffer *Canvas = nk_window_get_canvas(Ctx);

	BOOL bIsMouseDown = nk_input_is_mouse_down(&Ctx->input, NK_BUTTON_LEFT);
	BOOL bIsHoveringHandle = nk_input_is_mouse_hovering_rect(&Ctx->input, Bounds);
	State->bIsDraggingBottom = bIsMouseDown && (State->bIsDraggingBottom || bIsHoveringHandle);

	if (State->bIsDraggingBottom) {
		State->BottomPanelHeight -= Ctx->input.mouse.delta.y;
		nk_fill_rect(Canvas, Bounds, 0, nk_rgba(150, 150, 255, 255));
	} else if (bIsHoveringHandle) {
		nk_fill_rect(Canvas, Bounds, 0, nk_rgba(100, 100, 120, 200));
	} else {
		struct nk_rect line = nk_rect(Bounds.x + Bounds.w / 2 - 20, Bounds.y + 2, 40, 2);
		nk_fill_rect(Canvas, line, 1.0f, nk_rgba(60, 60, 60, 255));
	}
}

void
BottomPanelTabArea(struct nk_context *Ctx, const float TabBarHeight, UI_BottomPanelState *State)
{
	nk_layout_row_begin(Ctx, NK_STATIC, TabBarHeight, 4);
	{
		nk_layout_row_push(Ctx, TAB_WIDTH);

		struct nk_style_button TabStyle = Ctx->style.contextual_button;

		TabStyle.normal = State->ActiveTab == EBBS_LOG_TAB ? Ctx->style.button.hover : Ctx->style.button.normal;
		if (nk_button_label_styled(Ctx, &TabStyle, "Logs")) {
			State->ActiveTab = EBBS_LOG_TAB;
		}

		TabStyle.normal = State->ActiveTab == EBBS_SCENE_TAB ? Ctx->style.button.hover : Ctx->style.button.normal;
		if (nk_button_label_styled(Ctx, &TabStyle, "Models")) {
			State->ActiveTab = EBBS_SCENE_TAB;
		}

		TabStyle.normal = State->ActiveTab == EBBS_LIGHT_TAB ? Ctx->style.button.hover : Ctx->style.button.normal;
		if (nk_button_label_styled(Ctx, &TabStyle, "Lights")) {
			State->ActiveTab = EBBS_LIGHT_TAB;
		}
	}
	nk_layout_row_end(Ctx);
}

void
BottomPanelContentArea(struct nk_context *Ctx, UI_BottomPanelState *State, const float HandleHeight, const float TabBarHeight)
{
	float ContentAreaHeight = State->BottomPanelHeight - (HandleHeight + TabBarHeight + 10.0f);
	switch (State->ActiveTab) {
	case EBBS_LOG_TAB: {
		nk_layout_row_dynamic(Ctx, fmax(10.0f, ContentAreaHeight), 1);
		const nk_flags LogFlags = NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY | NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_GOTO_END_ON_ACTIVATE;

		int RequiredBytes =
			WideCharToMultiByte(CP_UTF8, 0, SENDAI_LOG.Buffer, SENDAI_LOG.Len, SENDAI_LOG.UTF8Buffer, sizeof(SENDAI_LOG.UTF8Buffer) - 1, NULL, NULL);
		if (RequiredBytes >= 0) {
			SENDAI_LOG.UTF8Buffer[RequiredBytes] = '\0';
			nk_edit_string(Ctx, LogFlags, SENDAI_LOG.UTF8Buffer, &RequiredBytes, sizeof(SENDAI_LOG.UTF8Buffer), nk_filter_default);
		}
		break;
	}
	case EBBS_SCENE_TAB: {
		float SafeHeight = fmax(50.0f, ContentAreaHeight);

		nk_layout_row_template_begin(Ctx, SafeHeight);
		nk_layout_row_template_push_variable(Ctx, 200);
		nk_layout_row_template_push_static(Ctx, 250);
		nk_layout_row_template_end(Ctx);

		if (nk_group_begin(Ctx, "SceneTableGroup", NK_WINDOW_BORDER)) {
			nk_layout_row_dynamic(Ctx, 22, 2);
			nk_label(Ctx, "Name", NK_TEXT_LEFT);
			nk_label(Ctx, "Visible", NK_TEXT_LEFT);

			for (int i = 0; i < State->Scene->ModelsCount; i++) {
				R_Model *Model = &State->Scene->Models[i];
				int IsSelected = (State->SelectedModelIndex == i);

				nk_layout_row_dynamic(Ctx, 20, 2);
				if (nk_selectable_label(Ctx, Model->Name, NK_TEXT_LEFT, &IsSelected)) {
					State->SelectedModelIndex = i;
				}
				nk_checkbox_label(Ctx, "", &Model->Visible);
			}
			nk_group_end(Ctx);
		}

		if (nk_group_begin(Ctx, "InspectorGroup", NK_WINDOW_BORDER)) {
			if (State->SelectedModelIndex >= 0 && State->SelectedModelIndex < State->Scene->ModelsCount) {
				R_Model *SelModel = &State->Scene->Models[State->SelectedModelIndex];

				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_label(Ctx, "Name:", NK_TEXT_LEFT);
				nk_label_colored(Ctx, SelModel->Name, NK_TEXT_LEFT, nk_rgb(255, 255, 0));

				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_label(Ctx, "Position:", NK_TEXT_LEFT);

				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_property_float(Ctx, "#X:", -1000.0f, &SelModel->Position.x, 1000.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Y:", -1000.0f, &SelModel->Position.y, 1000.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Z:", -1000.0f, &SelModel->Position.z, 1000.0f, 0.1f, 0.05f);

				nk_label(Ctx, "Rotation:", NK_TEXT_LEFT);
				nk_property_float(Ctx, "#X:", -100.0f, &SelModel->Rotation.x, 100.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Y:", -100.0f, &SelModel->Rotation.y, 100.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Z:", -100.0f, &SelModel->Rotation.z, 100.0f, 0.1f, 0.05f);

				nk_label(Ctx, "Scale:", NK_TEXT_LEFT);
				nk_property_float(Ctx, "#X:", 0.01f, &SelModel->Scale.x, 500.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Y:", 0.01f, &SelModel->Scale.y, 500.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Z:", 0.01f, &SelModel->Scale.z, 500.0f, 0.1f, 0.05f);

			} else {
				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_label(Ctx, "No model selected", NK_TEXT_CENTERED);
			}
			nk_group_end(Ctx);
		}
		break;
	}

	case EBBS_LIGHT_TAB: {
		float SafeHeight = fmax(50.0f, ContentAreaHeight);

		nk_layout_row_template_begin(Ctx, SafeHeight);
		nk_layout_row_template_push_variable(Ctx, 200);
		nk_layout_row_template_push_static(Ctx, 250);
		nk_layout_row_template_end(Ctx);

		if (nk_group_begin(Ctx, "LightTableGroup", NK_WINDOW_BORDER)) {
			nk_layout_row_dynamic(Ctx, 22, 2);
			nk_label(Ctx, "Name", NK_TEXT_LEFT);
			nk_label(Ctx, "Active", NK_TEXT_LEFT);

			for (int i = 0; i < 7; i++) {
				BOOL IsSelected = (State->SelectedLightIndex == i);
				char AsStr[12];
				sprintf_s(AsStr, sizeof(AsStr), "%d", i);
				nk_layout_row_dynamic(Ctx, 20, 2);
				BOOL IsActive = (State->Scene->ActiveLightMask >> i) & 1;
				if (IsActive) {
					if (nk_selectable_label(Ctx, AsStr, NK_TEXT_LEFT, &IsSelected)) {
						State->SelectedLightIndex = i;
					}
				} else {
					nk_style_push_color(Ctx, &Ctx->style.text.color, COLOR_DISABLED);
					nk_label(Ctx, AsStr, NK_TEXT_LEFT);
					nk_style_pop_color(Ctx);
				}
				if (nk_checkbox_label(Ctx, "", &IsActive)) {
					if (IsActive) {
						State->Scene->ActiveLightMask |= (1 << i);
					} else {
						State->Scene->ActiveLightMask &= ~(1 << i);
					}
				}
			}
			nk_group_end(Ctx);
		}

		if (nk_group_begin(Ctx, "InspectorGroup", NK_WINDOW_BORDER)) {
			if (State->SelectedLightIndex >= 0 && State->SelectedLightIndex < 7) {
				R_Light *SelectedLight = &State->Scene->Data.Lights[State->SelectedLightIndex];

				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_label(Ctx, "Position:", NK_TEXT_LEFT);

				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_property_float(Ctx, "#X:", -1000.0f, &SelectedLight->LightPosition.x, 1000.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Y:", -1000.0f, &SelectedLight->LightPosition.y, 1000.0f, 0.1f, 0.05f);
				nk_property_float(Ctx, "#Z:", -1000.0f, &SelectedLight->LightPosition.z, 1000.0f, 0.1f, 0.05f);

				nk_label(Ctx, "Color:", NK_TEXT_LEFT);
				nk_property_float(Ctx, "#R:", 0.0f, &SelectedLight->LightColor.x, 5000.0f, 10.0f, 1.0f);
				nk_property_float(Ctx, "#G:", 0.0f, &SelectedLight->LightColor.y, 5000.0f, 10.0f, 1.0f);
				nk_property_float(Ctx, "#B:", 0.0f, &SelectedLight->LightColor.z, 5000.0f, 10.0f, 1.0f);

			} else {
				nk_layout_row_dynamic(Ctx, 20, 1);
				nk_label(Ctx, "No model selected", NK_TEXT_CENTERED);
			}
			nk_group_end(Ctx);
		}
		break;
	}

	default: {
		nk_layout_row_dynamic(Ctx, 20, 1);
		nk_label(Ctx, "Select a tab to view content", NK_TEXT_LEFT);
		break;
	}
	}
}

void
BottomBarInfoArea(struct nk_context *Ctx, UI_Renderer *UI, const float InfoBarHeight, UI_BottomPanelState *State)
{
	nk_style_push_style_item(Ctx, &Ctx->style.window.fixed_background, nk_style_item_color(INFO_BAR_BACKGROUND_COLOR));
	nk_style_push_vec2(Ctx, &Ctx->style.window.padding, INFO_BAR_PADDING);
	nk_style_push_color(Ctx, &Ctx->style.text.color, INFO_BAR_TEXT_COLOR);
	struct nk_rect InfoRect = nk_rect(0, UI->Height - InfoBarHeight, UI->Width, InfoBarHeight);

	if (nk_begin(Ctx, "InfoBar", InfoRect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		nk_layout_row_begin(Ctx, NK_DYNAMIC, InfoBarHeight - 4, 2);
		nk_layout_row_push(Ctx, 0.85f);
		nk_label(Ctx, " Sendai Engine v0.1", NK_TEXT_LEFT);

		nk_layout_row_push(Ctx, 0.15f);
		char FPSBuffer[16];
		snprintf(FPSBuffer, sizeof(FPSBuffer), "%u FPS", State->FPS);
		nk_label(Ctx, FPSBuffer, NK_TEXT_RIGHT);
		nk_layout_row_end(Ctx);
	}
	nk_end(Ctx);
}