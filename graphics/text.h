#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../extras/simpledwrite.h"

namespace chaos {

class Engine;
class Texture;

namespace ui {

class TextRenderer {
 public:
  TextRenderer();
  ~TextRenderer();

  void Init(Engine* engine, const std::vector<std::string>& fonts);
  bool RenderImGui(const std::string& id, const std::string& text,
      simpledwrite::Layout* layout);

 private:
  Engine* engine_;
  std::unique_ptr<simpledwrite::SimpleDWrite> dwrite_;

  struct Cache {
    uint64_t hash = 0;
    std::shared_ptr<chaos::Texture> texture;
    bool rendering = false;
    simpledwrite::Layout layout;
  };
  std::unordered_map<std::string, Cache> cache_;
  std::mutex mutex_;
  std::mutex mutex_render_;
};

}  // namespace ui

}  // namespace chaos
