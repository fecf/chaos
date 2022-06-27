#pragma once

#include "d3d12def.h"

namespace chaos {

class SwapChain {
 public:
  SwapChain(ComPtr<IDXGIFactory7> factory, ComPtr<ID3D12Device8> device,
      ComPtr<ID3D12CommandQueue> command_queue, HWND hwnd, int width,
      int height, DXGI_FORMAT format, int back_buffer_count);
  ~SwapChain();

  int GetCurrentBackBufferIndex() const;
  ComPtr<ID3D12Resource> GetRenderTarget(int index) const;

  void Wait() const;
  HRESULT Present();
  HRESULT Resize(
      int back_buffer_count, int width, int height, DXGI_FORMAT format);
  void UpdateColorSpace(bool hdr);

 private:
  ComPtr<IDXGISwapChain4> swapchain_;
  std::vector<ComPtr<ID3D12Resource>> rendertargets_;
  DWORD flags_;
  HANDLE waitable_object_;
  DXGI_FORMAT format_;
  DXGI_COLOR_SPACE_TYPE colorspace_;
  bool hdr_;
};
}  // namespace chaos