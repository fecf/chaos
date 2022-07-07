#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace chaos {

namespace window_event {
struct Move {
  int x;
  int y;
};
struct Resize {
  int width;
  int height;
};
struct State {
  int state;
  int width;
  int height;
};
struct Fullscreen {
  bool enabled;
};
struct DragDrop {
  std::vector<std::string> value;
};
using window_event_t = std::variant<Move, Resize, State, Fullscreen, DragDrop>;
}  // namespace window_event

struct WindowConfig {
  WindowConfig();
  ~WindowConfig();

  std::string id;
  std::string title;
  int icon;
  int x, y, width, height;

  std::function<bool(window_event::window_event_t variant)> event_filter;
  std::function<bool(void* handle, int msg, uint64_t wparam, uint64_t lparam)>
      native_event_filter;
};

class Window {
 public:
  static const int kDefault;

  enum class State { Normal, Minimize, Maximize };
  struct Rect {
    int x, y, width, height;
  };

  virtual ~Window() noexcept;

  virtual void Show(State state) = 0;
  virtual bool Update() = 0;
  virtual void Move(int x, int y) = 0;
  virtual void Resize(int width, int height, bool client_size = true) = 0;

  virtual State GetState() const = 0;
  virtual void* GetHandle() const = 0;
  virtual Rect GetWindowRect() const = 0;
  virtual Rect GetClientRect() const = 0;
  virtual bool IsActive() const = 0;
  virtual bool IsTopmost() const = 0;
  virtual bool IsFrame() const = 0;
  virtual bool IsFullscreen() const = 0;
  virtual void SetTitle(const std::string& title) = 0;
  virtual void SetTopmost(bool enabled) = 0;
  virtual void SetFrame(bool enabled) = 0;
  virtual void SetFullScreen(bool enabled) = 0;
};

std::unique_ptr<Window> CreatePlatformWindow(const WindowConfig& config);

}  // namespace chaos
