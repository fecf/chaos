#include "engine.h"

#include <variant>

#include "../base/task.h"
#include "../graphics/imgui/imgui_impl_win32.h"
#include "../graphics/impl/context.h"
#include "../graphics/window.h"
#include "engine.h"

#ifdef IMGUI_VERSION
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

namespace chaos {

constexpr const char* kTextureImGuiFontAtlas = "imgui_font_atlas";

class DrawCommand {
 public:
  struct Draw {
    int element_count;
    int index_offset;
    int vertex_offset;
  };
  struct SetRootConstant {
    uint32_t slot;
    uint32_t value;
  };
  struct SetImGuiTextOutline {
    int offset;
  };
  struct SetImGuiTexture {
    uint64_t handle;
  };
  struct SetTexture {
    uint64_t handle;
  };
  using Command =
      std::variant<Draw, SetRootConstant, SetImGuiTextOutline, SetImGuiTexture>;
};

ImTextureID Texture::AsImGui() const {
  uint64_t descriptor_handle_index =
      static_cast<uint64_t>(resource_->GetDescriptorHandle().index);
  ImTextureID texture_id =
      reinterpret_cast<ImTextureID>(descriptor_handle_index);
  return texture_id;
}

Engine::Engine() {
  std::unique_ptr<Context> context(new Context());
  context_ = std::move(context);
}

Engine::~Engine() { Destroy(); }

bool Engine::Tick() {
  if (window_) {
    if (!window_->Update()) {
      return false;
    }
  }
  if (!render()) {
    return false;
  }
  return true;
}

void Engine::Destroy() {
  chaos::task::enumerateDispatchQueues(
      [](const char* name) { chaos::task::dispatchQueue(name)->wait(); });
  imgui_font_atlas_.reset();
  context_.reset();
  window_.reset();
}

void Engine::UseLogger() {
#ifdef _DEBUG
  minlog::add_sink(minlog::sink::cout());
  minlog::add_sink(minlog::sink::debug());
#endif
}

void Engine::UseWindow() {
  WindowConfig config;
  config.x = Window::kDefault;
  config.y = Window::kDefault;
  config.width = 960;
  config.height = 540;
  UseWindow(config);
}

void Engine::UseWindow(WindowConfig config) {
  config.event_filter =
      std::bind(&Engine::windowEventFilter, this, std::placeholders::_1);
  config.native_event_filter =
      std::bind(&Engine::windowNativeEventFilter, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

  std::unique_ptr<Window> window = CreatePlatformWindow(config);
  const HWND hwnd = (HWND)window->GetHandle();
  const Window::Rect rect = window->GetClientRect();
  context_->SetWindow(hwnd, rect.width, rect.height);
  window_ = std::move(window);
}

void Engine::UseImGui(std::function<bool()> imgui_render_func) {
  assert(window_);
  ImGui_ImplWin32_Init(window_->GetHandle());

  const ImGuiIO& io = ImGui::GetIO();
  int width = 0, height = 0, bpp = 0;
  unsigned char* pixels = NULL;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);
  assert(bpp == 4);

  std::vector<uint8_t> data(pixels, pixels + width * height * bpp);
  std::shared_ptr<Image> image(new Image(width, height, width * bpp,
      PixelFormat::RGBA8, 4, ColorSpace::sRGB, std::move(data)));

  if (width > 0 && height > 0 && pixels != NULL) {
    std::shared_ptr<Texture> font_atlas = CreateTexture(image);
    if (!font_atlas) {
      throw std::runtime_error("failed CreateTexture().");
    }
    io.Fonts->SetTexID(font_atlas->AsImGui());
    imgui_font_atlas_ = font_atlas;
  } else {
    throw std::runtime_error("failed GetTexDataAsRGBA32().");
  }

  if (!imgui_render_func) {
    throw std::runtime_error("invalid arg.");
  }
  imgui_render_func_ = imgui_render_func;
}

std::shared_ptr<Scene> Engine::GetScene() const { return nullptr; }

void Engine::SetScene(std::shared_ptr<Scene> scene) {}

Window* Engine::GetWindow() const { return window_.get(); }

void Engine::SetWindowEventFilter(
    std::function<bool(window_event::window_event_t data)> callback) {
  window_event_filter_ = callback;
}

void Engine::SetWindowNativeEventFilter(
    std::function<bool(int, uint64_t, uint64_t)> callback) {
  window_native_event_filter_ = callback;
}

std::shared_ptr<Texture> Engine::CreateTexture(const texture_source_t& source) {
  std::shared_ptr<Image> image;
  std::string metadata_source;
  if (std::holds_alternative<std::string>(source)) {
    std::string url = std::get<std::string>(source);
    image = Image::Load(url);
    metadata_source = url;
  } else if (std::holds_alternative<std::shared_ptr<Image>>(source)) {
    image = std::get<std::shared_ptr<Image>>(source);
  } else {
    throw std::runtime_error("unexpected type.");
  }

  const auto resource_format = ResourceFormat::FromPixelFormat(image->format());
  const auto resource_desc =
      chaos::ResourceDesc(resource_format, image->width(), image->height());
  auto resource = context_->CreateResource(resource_desc);
  context_->CopyResource2D(
      resource.get(), image->data(), image->stride(), image->height());

  std::shared_ptr<Texture> texture(new Texture());
  texture->width = image->width();
  texture->height = image->height();
  texture->format = image->format();
  texture->cs = image->colorspace();
  texture->resource_ = std::move(resource);
  texture->metadata.source = metadata_source;
  return texture;
}

bool Engine::render() {
  static bool rendering = false;
  if (rendering) {
    return true;
  }

  rendering = true;
  try {
    if (imgui_render_func_) {
      ImGui_ImplWin32_NewFrame();
      if (!imgui_render_func_()) {
        return false;
      }
    }

    context_->Prepare();
    context_->Render();
  } catch (std::exception& ex) {
    // unhandled exception.
    throw ex;
  }
  rendering = false;
  return true;
}

bool Engine::windowEventFilter(window_event::window_event_t data) {
  if (window_event_filter_) {
    window_event_filter_(data);
  }
  std::visit(overloaded{[&](window_event::DragDrop _) {},
                 [&](window_event::State _) {
                   context_->WindowSizeChanged(_.width, _.height);
                   render();
                 },
                 [&](window_event::Resize _) {
                   context_->WindowSizeChanged(_.width, _.height);
                   render();
                 },
                 [](const auto&) {}},
      data);
  return true;
}

bool Engine::windowNativeEventFilter(
    void* handle, int msg, uint64_t arg0, uint64_t arg1) {
  if (ImGui_ImplWin32_WndProcHandler((HWND)handle, msg, arg0, arg1)) {
    return true;
  }
  if (window_native_event_filter_) {
    if (window_native_event_filter_(msg, arg0, arg1)) {
      return true;
    }
  }
  return false;
}

Engine* engine() {
  static Engine engine;
  return &engine;
}

}  // namespace chaos
