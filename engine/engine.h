#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../base/types.h"
#include "../graphics/imguidef.h"
#include "../graphics/window.h"
#include "../image/image.h"

namespace chaos {

class Context;
class DrawCommand;
class Resource;
class ResourceDesc;

class Texture {
 public:
  friend class Engine;

  int width;
  int height;
  PixelFormat format;
  ColorSpace cs;
  struct {
    std::string source;
  } metadata;
  ImTextureID AsImGui() const;

 private:
  std::shared_ptr<Resource> resource_;
};

struct Scene {
  std::function<bool(Scene*)> imgui;
};

class Engine {
 public:
  Engine();
  ~Engine();

  bool Tick();
  void Destroy();

  void UseLogger();
  void UseWindow();
  void UseWindow(WindowConfig config);
  void UseImGui(std::function<bool()> imgui_render_func);

  std::shared_ptr<Scene> GetScene() const;
  void SetScene(std::shared_ptr<Scene> scene);

  Window* GetWindow() const;
  void SetWindowEventFilter(
      std::function<bool(window_event::window_event_t)> callback);
  void SetWindowNativeEventFilter(
      std::function<bool(int, uint64_t, uint64_t)> callback);

  using texture_source_t = std::variant<std::string, std::shared_ptr<Image>>;
  std::shared_ptr<Texture> CreateTexture(const texture_source_t& source);

 private:
  bool render();
  bool windowEventFilter(window_event::window_event_t data);
  bool windowNativeEventFilter(void*, int, uint64_t, uint64_t);

 private:
  std::unique_ptr<Window> window_;

  std::function<bool(window_event::window_event_t)> window_event_filter_;
  std::function<bool(int, uint64_t, uint64_t)> window_native_event_filter_;
  std::queue<std::function<void()>> tasks_;

  std::function<bool()> imgui_render_func_;
  std::shared_ptr<Texture> imgui_font_atlas_;

  std::unique_ptr<Context> context_;
  std::vector<DrawCommand> draw_commands_;
};

Engine* engine();

}  // namespace chaos