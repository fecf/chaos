#include "drawstate.h"

#include <stack>

#include "imguidef.h"

namespace chaos {

namespace ui {

namespace {

bool outline_ = false;
std::stack<bool> outline_stack_;

TextureFilter texture_filter_ = TextureFilter::Bilinear;
std::stack<TextureFilter> texture_filter_stack_;

ColorSpace src_color_space_ = ColorSpace::sRGB;
std::stack<ColorSpace> src_color_space_stack_;

ColorSpace dst_color_space_ = ColorSpace::Linear;
std::stack<ColorSpace> dst_color_space_stack_;

}  // namespace

void AddDrawOutlineCallback(bool enabled) {
  ImGui::GetCurrentWindow()->DrawList->AddCallback(
      (ImDrawCallback)chaos::kImDrawCallbackSetDrawOutline,
      reinterpret_cast<void*>(enabled ? 1ull : 0ull));
  outline_ = enabled;

  // HACK: To avoid IM_ASSERT in _OnChangedClipRect
#ifdef _DEBUG
  ImGui::GetCurrentWindow()->DrawList->AddDrawCmd();
#endif
}

void AddTextureFilterCallback(TextureFilter filter) {
  ImGui::GetCurrentWindow()->DrawList->AddCallback(
      (ImDrawCallback)kImDrawCallbackSetTextureFilter,
      reinterpret_cast<void*>(filter));
  texture_filter_ = filter;
}

void AddSrcColorSpaceCallback(ColorSpace cs) {
  ImGui::GetCurrentWindow()->DrawList->AddCallback(
      (ImDrawCallback)kImDrawCallbackSetSrcColorSpace,
      reinterpret_cast<void*>(cs));
  src_color_space_ = cs;
}

void AddDstColorSpaceCallback(ColorSpace cs) {
  ImGui::GetCurrentWindow()->DrawList->AddCallback(
      (ImDrawCallback)kImDrawCallbackSetDstColorSpace,
      reinterpret_cast<void*>(cs));
  dst_color_space_ = cs;
}

void PushOutline(bool enabled) {
  outline_stack_.push(outline_);
  AddDrawOutlineCallback(enabled);
}

void PopOutline() {
  assert(!outline_stack_.empty());
  AddDrawOutlineCallback(outline_stack_.top());
  outline_stack_.pop();
}

void PushTextureFilter(TextureFilter filter) {
  texture_filter_stack_.push(filter);
  AddTextureFilterCallback(filter);
}

void PopTextureFilter() {
  assert(!texture_filter_stack_.empty());
  AddTextureFilterCallback(texture_filter_stack_.top());
  texture_filter_stack_.pop();
}

void PushSrcColorSpace(ColorSpace cs) {
  src_color_space_stack_.push(cs);
  AddSrcColorSpaceCallback(cs);
}

void PopSrcColorSpace() {
  assert(!src_color_space_stack_.empty());
  AddSrcColorSpaceCallback(src_color_space_stack_.top());
  src_color_space_stack_.pop();
}

void PushDstColorSpace(ColorSpace cs) {
  dst_color_space_stack_.push(cs);
  AddDstColorSpaceCallback(cs);
}

void PopDstColorSpace() {
  assert(!dst_color_space_stack_.empty());
  AddDstColorSpaceCallback(dst_color_space_stack_.top());
  dst_color_space_stack_.pop();
}

}  // namespace ui

}  // namespace chaos
