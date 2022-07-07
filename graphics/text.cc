#include "text.h"

#include "base/task.h"
#include "engine/engine.h"
#include "drawstate.h"
#include "imguidef.h"

namespace chaos {

namespace ui {

TextRenderer::TextRenderer() {}

TextRenderer::~TextRenderer() { chaos::task::dispatchQueue("text")->wait(); }

void TextRenderer::Init(Engine* engine, const std::vector<std::string>& fonts) {
  assert(fonts.size());

  std::lock_guard lock(mutex_);

  cache_.clear();
  engine_ = engine;
  dwrite_ = std::unique_ptr<simpledwrite::SimpleDWrite>(new simpledwrite::SimpleDWrite());

  simpledwrite::FontSet fs;
  for (const std::string& font : fonts) {
    fs.fonts.push_back(simpledwrite::Font(font));
  }

  bool ret = dwrite_->Init(fs, (float)::GetSystemDpiForProcess(::GetCurrentProcess()));
  if (!ret) {
    throw std::runtime_error("failed SimpleDWrite::Init() " + dwrite_->GetLastError());
  }
}

bool TextRenderer::RenderImGui(const std::string& id, const std::string& text, simpledwrite::Layout* layout) {
  std::lock_guard lock(mutex_);

  uint64_t hash = std::hash<std::string>()(text);
  if (cache_.count(id)) {
    const auto& cache = cache_.at(id);
    if (cache.hash == hash) {
      if (cache.layout.font_size == layout->font_size) {
        if (cache.rendering) {
          ImGui::Dummy(
              {(float)cache.layout.font_size, (float)cache.layout.font_size});
          return false;
        } else {
          std::shared_ptr<Texture> texture = cache.texture;
          if (texture) {
            PushOutline(false);
            ImGui::Image(texture->AsImGui(),
                {(float)texture->width, (float)texture->height});
            PopOutline();
            *layout = cache.layout;
            return true;
          }
        }
      }
    }
  }

  Cache& cache = cache_[id];
  cache.rendering = true;
  cache.hash = hash;
  cache.layout = *layout;

  task::dispatchAsync("text", [=](std::atomic<bool>&) {
    thread_local std::vector<uint8_t> buffer_;

    simpledwrite::Layout layout;
    {
      std::lock_guard lock(mutex_);
      layout = cache_[id].layout;
    }

    {
      std::lock_guard lock(mutex_render_);
      bool ret = dwrite_->CalcSize(text, layout);
      if (!ret) {
        throw std::runtime_error(
            "failed SimpleDWrite::CalcSize() " + dwrite_->GetLastError());
      }

      if (!layout.out_width || !layout.out_height) {
        return true;
      }

      if (buffer_.size() < layout.out_buffer_size) {
        buffer_.resize(layout.out_buffer_size);
      }

      simpledwrite::RenderParams renderparams;
      renderparams.outline_width = 2.0f;
      renderparams.outline_color =
          simpledwrite::Color(0.33f, 0.33f, 0.33f, 0.8f);
      renderparams.foreground_color = simpledwrite::Color(1, 1, 1, 0.96f);
      renderparams.background_color = simpledwrite::Color(0, 0, 0, 0);
      ret = dwrite_->Render(
          text, buffer_.data(), (int)buffer_.size(), layout, renderparams);
      if (!ret) {
        throw std::runtime_error(
            "failed SimpleDWrite::Render() " + dwrite_->GetLastError());
      }
    }

    std::shared_ptr<chaos::Image> image(new Image(layout.out_width,
        layout.out_height, layout.out_width * 4, chaos::PixelFormat::BGRA8, 4,
        chaos::ColorSpace::sRGB, std::move(buffer_)));
    std::string texture_id = "text_" + std::to_string(hash);
    std::shared_ptr<chaos::Texture> texture = engine_->CreateTexture(image);
    if (!texture) {
      throw std::runtime_error("failed CreateTextureMaterial()");
    }

    {
      std::lock_guard lock(mutex_);
      cache_[id].texture = texture;
      cache_[id].rendering = false;
      cache_[id].layout = layout;
    }
    return true;
  });

  return false;
}

}  // namespace ui

}  // namespace chaos
