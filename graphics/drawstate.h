#pragma once

#include "../base/types.h"

namespace chaos {

namespace ui {

void AddDrawOutlineCallback(bool enabled);
void AddTextureFilterCallback(TextureFilter filter);
void AddSrcColorSpaceCallback(ColorSpace cs);
void AddDstColorSpaceCallback(ColorSpace cs);

void PushOutline(bool enabled);
void PopOutline();
void PushTextureFilter(TextureFilter filter);
void PopTextureFilter();
void PushSrcColorSpace(ColorSpace cs);
void PopSrcColorSpace();
void PushDstColorSpace(ColorSpace cs);
void PopDstColorSpace();

}  // namespace ui

}  // namespace chaos
