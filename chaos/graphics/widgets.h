#pragma once

#include "../base/types.h"
#include "imguidef.h"

namespace chaos {

namespace ui {

namespace widget {

const ImVec2 kIconPadding = {8.0f, 8.0f};
const ImVec2 kIconMargin = {4.0f, 4.0f};

void FillItem(const ImVec2& size, ImU32 col, float border_thickness = 0.0f,
    ImU32 border_col = 0x0, float rounding = 0.0f, float margin = 0.0f);
void FillItem(ImU32 col, float border_thickness = 0.0f, ImU32 border_col = 0x0,
    float rounding = 0.0f, float margin = 0.0f);

bool RadioButton(const char* label, bool checked);
bool CheckBox(const char* label, bool checked);
bool MenuItem(const char* label, bool checked = false);
void Spinner(
    float radius, float thickness, int num_segments, float speed, ImU32 color);
void Frame(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding);

bool TextButton(const char* label, bool toggled = false,
    const ImVec2& preferred_size = ImVec2(), ImU32 bg = 0x00000000);
bool PushButton(const char* label, bool toggled = false,
    const ImVec2& preferred_size = ImVec2());

void HyperLink(const char* url, const char* label = nullptr);
void DrawUnderline(int y = 0);
bool TreeHeader(const char* label, bool closed = false);

}  // namespace widget

}  // namespace ui

}  // namespace chaos
