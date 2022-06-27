#include "window.h"

#include "base/win32def.h"

namespace chaos {

const int Window::kDefault = INT32_MAX;

WindowConfig::WindowConfig()
    : id("Unnamed"),
      title("Unnamed"),
      icon(),
      x(Window::kDefault),
      y(Window::kDefault),
      width(Window::kDefault),
      height(Window::kDefault) {}

WindowConfig::~WindowConfig() {}

namespace windows {

class DropTarget : public IDropTarget {
 public:
  DropTarget() = delete;
  DropTarget(std::function<void(const std::vector<std::string>&)> callback)
      : callback_(callback) {}

  ULONG AddRef() { return 1; }
  ULONG Release() { return 0; }
  HRESULT QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IDropTarget) {
      *ppvObject = this;
      return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  HRESULT DragEnter(
      IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect &= DROPEFFECT_COPY;
    return S_OK;
  }
  HRESULT DragLeave() { return S_OK; }
  HRESULT DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect &= DROPEFFECT_COPY;
    return S_OK;
  }
  HRESULT Drop(
      IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    FORMATETC fmte = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stgm;
    if (SUCCEEDED(pDataObj->GetData(&fmte, &stgm))) {
      HDROP hdrop = (HDROP)stgm.hGlobal;
      UINT file_count = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
      for (UINT i = 0; i < file_count; i++) {
        TCHAR szFile[MAX_PATH];
        UINT cch = DragQueryFile(hdrop, i, szFile, MAX_PATH);
        std::vector<std::string> paths;
        if (cch > 0 && cch < MAX_PATH) {
          paths.push_back(szFile);
        }
        if (paths.size()) {
          callback_(paths);
        }
      }
      ::ReleaseStgMedium(&stgm);
    }
    *pdwEffect &= DROPEFFECT_COPY;
    return S_OK;
  }

 private:
  std::function<void(const std::vector<std::string>&)> callback_;
};

LRESULT CALLBACK staticWindowProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

class Window : public ::chaos::Window {
 public:
  friend LRESULT CALLBACK staticWindowProc(
      HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

  Window() = delete;
  Window(const WindowConfig& config)
      : hwnd_(),
        config_(config),
        wp_(),
        client_rect_(),
        state_(State::Normal),
        active_(false),
        moving_(false),
        topmost_(false),
        frame_(true),
        fullscreen_(false),
        drop_target_() {
    ::OleInitialize(NULL);

    // register window class
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    // wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.style = 0;
    wc.lpfnWndProc = chaos::windows::staticWindowProc;
    wc.hInstance = ::GetModuleHandle(NULL);
    wc.hIcon = ::LoadIcon(wc.hInstance, MAKEINTRESOURCE(config.icon));
    wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = config.id.c_str();
    ATOM atom = ::RegisterClassEx(&wc);
    if (atom == NULL) {
      throw std::runtime_error("falied RegisterClassEx().");
    }

    // create window
    const HWND parent = NULL;
    const HMENU menu = NULL;
    const HINSTANCE instance = ::GetModuleHandle(NULL);
    const LPVOID param = static_cast<LPVOID>(this);
    const DWORD exstyle = 0;
    const DWORD style = WS_OVERLAPPEDWINDOW;
    int x = config.x == kDefault ? CW_USEDEFAULT : config.x;
    int y = config.y == kDefault ? CW_USEDEFAULT : config.y;
    int width = config.width == kDefault ? CW_USEDEFAULT : config.width;
    int height = config.height == kDefault ? CW_USEDEFAULT : config.height;
    HWND hwnd =
        ::CreateWindowEx(exstyle, MAKEINTATOM(atom), config.title.c_str(),
            style, x, y, width, height, parent, menu, instance, param);
    if (hwnd == NULL) {
      throw std::runtime_error("failed ::CreateWindowEx().");
    }
    hwnd_ = hwnd;

    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    if (!::GetWindowPlacement(hwnd, &wp)) {
      throw std::runtime_error("failed ::GetWindowPlacement().");
    }
    wp_ = wp;

    RECT rect;
    ::GetWindowRect(hwnd, &rect);
    window_rect_.x = rect.left;
    window_rect_.y = rect.top;
    window_rect_.width = rect.right - rect.left;
    window_rect_.height = rect.bottom - rect.top;
    ::GetClientRect(hwnd, &rect);
    client_rect_.x = rect.left;
    client_rect_.y = rect.top;
    client_rect_.width = rect.right - rect.left;
    client_rect_.height = rect.bottom - rect.top;

    if (wp.showCmd == SW_NORMAL) {
      state_ = State::Normal;
    } else if (wp.showCmd == SW_MAXIMIZE) {
      state_ = State::Maximize;
    } else if (wp.showCmd == SW_MINIMIZE) {
      state_ = State::Minimize;
    }

    drop_target_ = std::unique_ptr<DropTarget>(
        new DropTarget([&](const std::vector<std::string>& value) {
          config_.event_filter(window_event::DragDrop{value});
        }));
    HRESULT hr = ::RegisterDragDrop(hwnd, drop_target_.get());
    if (FAILED(hr)) {
      throw std::runtime_error("failed ::RegisterDragDrop().");
    }
  }

  virtual ~Window() noexcept {
    if (hwnd_ != NULL) {
      ::RevokeDragDrop(hwnd_);
      ::DestroyWindow(hwnd_);
    }

    ::OleUninitialize();
  }

  // Inherited via IPlatformWindow
  virtual void Show(State state) override {
    if (state == State::Normal) {
      ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
      ::UpdateWindow(hwnd_);
    } else if (state == State::Maximize) {
      ::ShowWindow(hwnd_, SW_SHOWMAXIMIZED);
      ::UpdateWindow(hwnd_);
    } else if (state == State::Minimize) {
      HWND next = ::GetWindow(hwnd_, GW_HWNDNEXT);
      while (true) {
        HWND temp = ::GetParent(next);
        if (!temp) break;
        next = temp;
      }
      ::ShowWindow(hwnd_, SW_SHOWMINIMIZED);
      ::SetForegroundWindow(next);
    }
  }
  virtual bool Update() override {
#if 1
  MSG msg{};
  while (msg.message != WM_QUIT) {
    if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    } else {
      break;
    }
  }
  return msg.message != WM_QUIT;
#else
    MSG msg{};
    if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    return msg.message != WM_QUIT;
#endif
  }

  virtual void Move(int x, int y) override {
    if (x == window_rect_.x && y == window_rect_.y) return;
    if (state_ != State::Normal) return;
    if (fullscreen_) return;

    BOOL ret = ::SetWindowPos(hwnd_, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    if (!ret) {
      throw std::runtime_error("failed SetWindowPos().");
    }
  }
  virtual void Resize(int width, int height, bool client_size) override {
    if (state_ != State::Normal) return;
    if (fullscreen_) return;

    if (client_size) {
      RECT rect{window_rect_.x, window_rect_.y,
          window_rect_.x + window_rect_.width,
          window_rect_.y + window_rect_.height};
      ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }
    if (width == window_rect_.width && width == window_rect_.width) return;

    BOOL ret = ::SetWindowPos(hwnd_, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
    if (!ret) {
      throw std::runtime_error("failed SetWindowPos().");
    }
  }
  virtual State GetState() const override { return state_; }
  virtual void* GetHandle() const override { return (void*)hwnd_; }
  virtual Rect GetWindowRect() const override { return window_rect_; }
  virtual Rect GetClientRect() const override { return client_rect_; }
  virtual bool IsActive() const override { return active_; }
  virtual bool IsTopmost() const override { return topmost_; }
  virtual bool IsFrame() const override { return frame_; }
  virtual bool IsFullscreen() const override { return fullscreen_; }
  virtual void SetTitle(const std::string& title) override {
    ::SetWindowText(hwnd_, title.c_str());
  }
  virtual void SetTopmost(bool enabled) override {
    ::SetWindowPos(hwnd_, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE);
    topmost_ = enabled;
  }
  virtual void SetFrame(bool enabled) override {
    if (fullscreen_) return;

    frame_ = enabled;  // Set this flag first
    LONG_PTR style = ::GetWindowLongPtr(hwnd_, GWL_STYLE);
    BOOL ret = TRUE;
    if (enabled) {
      style |= WS_VISIBLE;
      style &= ~WS_OVERLAPPEDWINDOW;
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);
      wp_ = {sizeof(WINDOWPLACEMENT)};
      ::GetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
      return;
    } else {
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
      ::SetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
              SWP_FRAMECHANGED);
      return;
    }
  }
  virtual void SetFullScreen(bool enabled) override {
    LONG_PTR style = ::GetWindowLongPtr(hwnd_, GWL_STYLE);
    BOOL ret = TRUE;
    if (enabled) {
      fullscreen_ = enabled;
      style |= WS_VISIBLE;
      style &= ~WS_OVERLAPPEDWINDOW;
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);

      wp_ = {sizeof(WINDOWPLACEMENT)};
      ::GetWindowPlacement(hwnd_, &wp_);
      HMONITOR monitor = ::MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi{sizeof(mi)};
      ::GetMonitorInfo(monitor, &mi);
      ::SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
          mi.rcMonitor.right - mi.rcMonitor.left,
          mi.rcMonitor.bottom - mi.rcMonitor.top,
          SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
      config_.event_filter(window_event::Fullscreen{true});
      return;
    } else {
      if (frame_) {
        style |= WS_OVERLAPPEDWINDOW;
      }
      ::SetWindowLongPtr(hwnd_, GWL_STYLE, style);
      ::SetWindowPlacement(hwnd_, &wp_);
      ::SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
              SWP_FRAMECHANGED);
      fullscreen_ = false;
      config_.event_filter(window_event::Fullscreen{false});
      return;
    }
  }

  LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (config_.native_event_filter) {
      bool ret = config_.native_event_filter(hwnd, msg, wparam, lparam);
      if (ret) {
        return TRUE;
      }
    }

    switch (msg) {
      case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
      case WM_ACTIVATE:
        active_ = (wparam == 1 || wparam == 2);
        break;
      case WM_WINDOWPOSCHANGED: {
        WINDOWPLACEMENT wndpl{sizeof(WINDOWPLACEMENT)};
        ::GetWindowPlacement(hwnd, &wndpl);
        State state = state_;
        if (wndpl.showCmd == SW_NORMAL) {
          state = State::Normal;
        } else if (wndpl.showCmd == SW_MAXIMIZE) {
          state = State::Maximize;
        } else if (wndpl.showCmd == SW_MINIMIZE) {
          state = State::Minimize;
        }
        if (state_ != state) {
          state_ = state;
          RECT rect{};
          ::GetClientRect(hwnd, &rect);
          int width = rect.right - rect.left;
          int height = rect.bottom - rect.top;
          if (state == State::Maximize || state == State::Normal) {
            config_.event_filter(window_event::Resize{width, height});
          }
          config_.event_filter(window_event::State{(int)state_, width, height});
        }
        break;
      }
      case WM_MOVE: {
        RECT rect{};
        ::GetWindowRect(hwnd, &rect);
        if (state_ == State::Normal && !fullscreen_) {
          window_rect_.x = rect.left;
          window_rect_.y = rect.top;
          window_rect_.width = rect.right - rect.left;
          window_rect_.height = rect.bottom - rect.top;
        }
        config_.event_filter(window_event::Move{rect.left, rect.top});
        break;
      }
      case WM_PAINT:
        PAINTSTRUCT ps;
        (void)::BeginPaint(hwnd, &ps);
        ::EndPaint(hwnd, &ps);
        break;
      case WM_SIZE: {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        if (state_ == State::Normal && !fullscreen_) {
          client_rect_.width = width;
          client_rect_.height = height;
        }
        config_.event_filter(window_event::Resize{width, height});
        break;
      }
      case WM_ERASEBKGND:
        return TRUE;
      case WM_ENTERSIZEMOVE:
        moving_ = true;
        break;
      case WM_EXITSIZEMOVE: {
        moving_ = false;
        RECT rect{};
        ::GetClientRect(hwnd, &rect);
        client_rect_.width = rect.right - rect.left;
        client_rect_.height = rect.bottom - rect.top;
        config_.event_filter(
            window_event::Resize{client_rect_.width, client_rect_.height});
        break;
      }
      case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) {  // Disable ALT application menu
          return 0;
        }
        break;
    }

    return ::DefWindowProc(hwnd, msg, wparam, lparam);
  }

 private:
  WindowConfig config_;
  HWND hwnd_;
  WINDOWPLACEMENT wp_;
  Rect window_rect_;
  Rect client_rect_;
  State state_;
  bool active_;
  bool moving_;
  bool topmost_;
  bool frame_;
  bool fullscreen_;
  std::unique_ptr<DropTarget> drop_target_;
};

LRESULT CALLBACK staticWindowProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_CREATE) {
    Window* impl = (Window*)((LPCREATESTRUCT)lparam)->lpCreateParams;
    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)impl);
  }

  Window* impl = (Window*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  return impl ? impl->windowProc(hwnd, msg, wparam, lparam)
              : ::DefWindowProc(hwnd, msg, wparam, lparam);
}

}  // namespace windows

Window::~Window() {}

std::unique_ptr<Window> CreatePlatformWindow(const WindowConfig& config) {
  return std::unique_ptr<Window>(new windows::Window(config));
}

}  // namespace chaos
