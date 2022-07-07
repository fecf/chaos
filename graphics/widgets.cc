#include "widgets.h"

#include "drawstate.h"

namespace chaos {

namespace ui {

namespace widget {

void FillItem(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding, float margin) {
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 r_min = ImGui::GetItemRectMin() + ImVec2(margin, margin);
  const ImVec2 r_max = ImGui::GetItemRectMin() + size - ImVec2(margin, margin);
  dd->AddRectFilled(r_min, r_max, col, rounding);
  if (border_thickness > 0.0f) {
    dd->AddRect(r_min, r_max, border_col, rounding, 0, border_thickness);
  }
}

void FillItem(ImU32 col, float border_thickness, ImU32 border_col,
    float rounding, float margin) {
  const ImVec2 size = ImGui::GetItemRectSize();
  FillItem(ImGui::GetItemRectSize(), col, border_thickness, border_col,
      rounding, margin);
}

bool RadioButton(const char* label, bool checked) {
  ImVec2 preferred_size = ImGui::CalcTextSize(label) + ImVec2{32.0f, 0.0f};
  const float width =
      std::max(preferred_size.x, ImGui::GetContentRegionAvail().x);
  const float height = ImGui::GetFontSize();
  PushOutline(false);

  // InvisibleButton
  ImGui::PushID(label);
  bool clicked = ImGui::InvisibleButton("", {width, height});
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    FillItem(ImGui::GetColorU32(ImGuiCol_HeaderHovered), 0, 0, 0, -1.0f);
  }

  // Radio
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 start = ImGui::GetItemRectMin();
  const ImVec2 end = start + ImVec2(height, height);
  const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
  const ImVec2 center = start + (end - start) / 2.0f;
  const float radius = (end.x - start.x) / 2.0f - 2.0f;
  const float radius_check = radius - 2.0f;
  dd->AddCircle(center, radius, col);
  if (checked) {
    dd->AddCircleFilled(center, radius_check, col);
  }

  // Text
  PushOutline(true);
  dd->AddText(start + ImVec2{height + 6.0f, 0},
      ImGui::GetColorU32(ImGuiCol_Text), label);
  PopOutline();

  PopOutline();
  return clicked;
}

bool CheckBox(const char* label, bool checked) {
  ImVec2 preferred_size = ImGui::CalcTextSize(label) + ImVec2{32.0f, 0.0f};
  const float width =
      std::max(preferred_size.x, ImGui::GetContentRegionAvail().x);
  const float height = ImGui::GetFontSize();
  PushOutline(false);

  // InvisibleButton
  ImGui::PushID(label);
  bool clicked = ImGui::InvisibleButton("", {width, height});
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    FillItem(ImGui::GetColorU32(ImGuiCol_HeaderHovered), 0, 0, 0, -1.0f);
  }

  // Check
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const float margin = 2.0f;
  const ImVec2 start = ImGui::GetItemRectMin() + ImVec2(margin, margin);
  const ImVec2 end =
      ImGui::GetItemRectMin() + ImVec2(height, height) - ImVec2(margin, margin);
  const float w = end.x - start.x;
  const float h = end.y - start.y;
  const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
  dd->AddRect(start, end, col);  // border
  if (checked) {
    const float pad_x = 3.0f;
    const float pad_y = 2.5f;
    const ImVec2 s1(start.x + pad_x, end.y - pad_y - h * 0.25f);
    const ImVec2 e1(start.x + pad_x + w * 0.25f, end.y - pad_y);
    const ImVec2 e2(start.x - pad_x + w, start.y + h * 0.1f);
    dd->AddLine(s1, e1, col, 1.5f);
    dd->AddLine(e1, e2, col, 1.5f);
  }

  // Text
  PushOutline(true);
  dd->AddText(start + ImVec2{height + 6.0f - margin, -margin},
      ImGui::GetColorU32(ImGuiCol_Text), label);
  PopOutline();

  PopOutline();
  return clicked;
}

bool MenuItem(const char* label, bool checked) {
  ImVec2 preferred_size = ImGui::CalcTextSize(label) + ImVec2{32.0f, 0.0f};
  const float width =
      std::max(preferred_size.x, ImGui::GetContentRegionAvail().x);
  const float height = ImGui::GetFontSize();
  PushOutline(false);

  // InvisibleButton
  ImGui::PushID(label);
  bool clicked = ImGui::InvisibleButton("", {width, height});
  ImGui::PopID();
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    FillItem(ImGui::GetColorU32(ImGuiCol_HeaderHovered), 0, 0, 0, -1.0f);
  }

  // Check
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const float margin = 2.0f;
  const ImVec2 start = ImGui::GetItemRectMin() + ImVec2(margin, margin);
  const ImVec2 end =
      ImGui::GetItemRectMin() + ImVec2(height, height) - ImVec2(margin, margin);
  const float w = end.x - start.x;
  const float h = end.y - start.y;
  const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
  if (checked) {
    const float pad_x = 3.0f;
    const float pad_y = 2.5f;
    const ImVec2 s1(start.x + pad_x, end.y - pad_y - h * 0.25f);
    const ImVec2 e1(start.x + pad_x + w * 0.25f, end.y - pad_y);
    const ImVec2 e2(start.x - pad_x + w, start.y + h * 0.1f);
    dd->AddLine(s1, e1, col, 1.5f);
    dd->AddLine(e1, e2, col, 1.5f);
  }

  // Text
  PushOutline(true);
  dd->AddText(start + ImVec2{height + 6.0f, 0},
      ImGui::GetColorU32(ImGuiCol_Text), label);
  PopOutline();

  PopOutline();
  return clicked;
}

void Spinner(
    float radius, float thickness, int num_segments, float speed, ImU32 color) {
  ImDrawList* dd = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImVec2 size{radius * 2, radius * 2};
  ImRect bb{pos, {pos.x + size.x, pos.y + size.y}};
  ImGui::ItemSize(bb);
  if (!ImGui::ItemAdd(bb, 0)) return;

  float time = static_cast<float>(ImGui::GetCurrentContext()->Time) * speed;
  dd->PathClear();
  int start = static_cast<int>(abs(ImSin(time) * (num_segments - 5)));
  float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
  float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;
  ImVec2 centre = {pos.x + radius, pos.y + radius};
  for (int i = 0; i < num_segments; ++i) {
    float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
    dd->PathLineTo({centre.x + ImCos(a + time * 8) * radius,
        centre.y + ImSin(a + time * 8) * radius});
  }
  dd->PathStroke(color, false, thickness);
}

void Frame(const ImVec2& size, ImU32 col, float border_thickness,
    ImU32 border_col, float rounding) {
  ImGui::PushID("##frame");
  ImGui::InvisibleButton("", size);
  ImGui::PopID();
  PushOutline(false);
  FillItem(col, 1.0f, 0x70707070, 3.0f, 1.0f);
  PopOutline();
}

bool TextButton(
    const char* label, bool toggled, const ImVec2& preferred_size, ImU32 bg) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  ImDrawList* dd = ImGui::GetWindowDrawList();
  const ImVec2 text_size = ImGui::CalcTextSize(label, NULL, true);
  const ImVec2 actual_size = {std::max(preferred_size.x, text_size.x),
      std::max(preferred_size.y, text_size.y)};

  PushOutline(false);
  ImGui::InvisibleButton(label, actual_size + kIconPadding);
  bool clicked = ImGui::IsItemClicked();
  FillItem(bg, 0, 0, 5.0f, 0);

  bool hovered = ImGui::IsItemHovered();
  ImU32 fg = hovered   ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
             : toggled ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                       : ImGui::GetColorU32(ImGuiCol_Text);
  if (hovered) {
    FillItem(0x80404040, 0, 0, 5.0f);
  }
  PopOutline();

  ImVec2 r0 = ImGui::GetItemRectMin() + kIconPadding / 2.0f;
  PushOutline(true);
  dd->AddText(ImGui::GetItemRectMin() + kIconPadding / 2.0f, fg, label);
  PopOutline();

  return clicked;
}

bool PushButton(const char* label, bool toggled, const ImVec2& preferred_size) {
  bool ret = TextButton(
      label, toggled, preferred_size, ImGui::GetColorU32(ImGuiCol_Button));
  return ret;
}

void HyperLink(const char* url, const char* label) {
  ImGui::Text("%s", label ? label : url);
  if (ImGui::GetHoveredID() == ImGui::GetItemID()) {
    if (ImGui::IsItemHovered()) {
      DrawUnderline();
    }
  }
}

void DrawUnderline(int y) {
  ImDrawList* dd = ImGui::GetCurrentWindow()->DrawList;
  PushOutline(false);
  ImVec2 ir0 = ImGui::GetItemRectMin();
  ImVec2 ir1 = ImGui::GetItemRectMax();
  if (y != 0) {
    float a = ir0.y;
    ir0.y = a + (float)y;
    ir1.y = a + (float)y;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  dd->AddLine({ir0.x, ir1.y - 1.0f}, {ir1.x, ir1.y - 1.0f}, 0x70f0f0f0);
  dd->AddLine({ir0.x, ir1.y - 0.0f}, {ir1.x, ir1.y - 0.0f}, 0x70101010);
  PopOutline();
}

bool TreeHeader(const char* label, bool closed) {
  const ImVec2 text_size = ImGui::CalcTextSize(label);
  const ImVec2 base = ImGui::GetCursorScreenPos();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4());
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 2.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  int flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
  if (!closed) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }
  PushOutline(false);
  bool opened = ImGui::TreeNodeBehavior(ImGui::GetID(label), flags, label);
  PopOutline();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();

  if (ImGui::IsItemVisible()) {
    const ImVec2 after = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(base);
    const ImVec2 size = ImGui::GetItemRectSize();
    constexpr float left_margin = 10.0f;

    // draw chevron
    ImDrawList* dd = ImGui::GetCurrentWindow()->DrawList;
    ImVec2 chevron = {base.x + left_margin, base.y + size.y / 2.0f};
    const ImU32 color =
        ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
    constexpr float thickness = 1.25;
    if (opened) {
      constexpr float w = 4.25f;
      constexpr float h = 2.25f;
      const ImVec2 points[3]{{chevron.x - w, chevron.y - h},
          {chevron.x, chevron.y + h}, {chevron.x + w, chevron.y - h}};
      dd->AddPolyline(points, IM_ARRAYSIZE(points), ImGui::GetColorU32(color),
          0, thickness);
    } else {
      constexpr float w = 2.25f;
      constexpr float h = 4.25f;
      const ImVec2 points[3]{
          {chevron.x - w, chevron.y - h},
          {chevron.x + w, chevron.y},
          {chevron.x - w, chevron.y + h},
      };
      dd->AddPolyline(points, IM_ARRAYSIZE(points), color, 0, thickness);
    }

    // draw text
    ImVec2 text = {base.x + left_margin + 12.0f,
        base.y + size.y / 2.0f - text_size.y / 2.0f};
    ImGui::RenderText(text, label, NULL);
    ImGui::SetCursorScreenPos(after);
  }
  return opened;
}

}  // namespace widget

}  // namespace ui

}  // namespace chaos

