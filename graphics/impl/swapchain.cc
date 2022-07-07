#include "swapchain.h"

namespace chaos {

SwapChain::SwapChain(ComPtr<IDXGIFactory7> factory,
    ComPtr<ID3D12Device8> device, ComPtr<ID3D12CommandQueue> command_queue,
    HWND hwnd, int width, int height, DXGI_FORMAT format, int back_buffer_count)
    : flags_(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT),
      waitable_object_(NULL),
      format_(format),
      colorspace_(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
      hdr_(false) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width;
  desc.Height = height;
  desc.Format = format;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = back_buffer_count;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Scaling = DXGI_SCALING_NONE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags = flags_;

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsdesc{};
  fsdesc.Windowed = TRUE;

  // Create a swap chain for the window.
  ComPtr<IDXGISwapChain1> swapchain;
  ThrowIfFailed(factory->CreateSwapChainForHwnd(command_queue.Get(), hwnd,
      &desc, &fsdesc, nullptr, swapchain.GetAddressOf()));
  ThrowIfFailed(swapchain.As(&swapchain_));
  waitable_object_ = swapchain_->GetFrameLatencyWaitableObject();

  // Disable alt+enter.
  ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

  // Handle color space settings for HDR
  UpdateColorSpace(hdr_);

  rendertargets_.resize(back_buffer_count);
  for (int i = 0; i < back_buffer_count; ++i) {
    ThrowIfFailed(swapchain_->GetBuffer(
        i, IID_PPV_ARGS(rendertargets_[i].GetAddressOf())));

    wchar_t name[25]{};
    swprintf_s(name, L"RenderTarget%u", i);
    rendertargets_[i]->SetName(name);
  }
}

SwapChain::~SwapChain() {}

void SwapChain::Wait() const {
  assert(waitable_object_ != NULL);
  ::WaitForSingleObjectEx(waitable_object_, 1000, true);
}

HRESULT SwapChain::Present() {
  assert(swapchain_ != NULL);
  HRESULT ret = swapchain_->Present(1, 0);
  return ret;
}

ComPtr<ID3D12Resource> SwapChain::GetRenderTarget(int index) const {
  assert(index < (int)rendertargets_.size());
  return rendertargets_.at(index);
}

int SwapChain::GetCurrentBackBufferIndex() const {
  return (int)swapchain_->GetCurrentBackBufferIndex();
}

HRESULT SwapChain::Resize(
    int back_buffer_count, int width, int height, DXGI_FORMAT format) {
  rendertargets_.resize(back_buffer_count);
  for (auto& rt : rendertargets_) {
    rt.Reset();
  }
  HRESULT hr = swapchain_->ResizeBuffers(
      0, width, height, DXGI_FORMAT_UNKNOWN, flags_);

  for (int i = 0; i < back_buffer_count; ++i) {
    ThrowIfFailed(swapchain_->GetBuffer(
        i, IID_PPV_ARGS(rendertargets_[i].GetAddressOf())));

    wchar_t name[25]{};
    swprintf_s(name, L"RenderTarget%u", i);
    rendertargets_[i]->SetName(name);
  }

  return hr;
}

void SwapChain::UpdateColorSpace(bool hdr) {
  bool supported_hdr10 = false;

  ComPtr<IDXGIOutput> output;
  if (SUCCEEDED(swapchain_->GetContainingOutput(output.GetAddressOf()))) {
    ComPtr<IDXGIOutput6> output6;
    if (SUCCEEDED(output.As(&output6))) {
      DXGI_OUTPUT_DESC1 desc;
      ThrowIfFailed(output6->GetDesc1(&desc));

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        // Display output is HDR10.
        supported_hdr10 = true;
      }
    }
  }

  DXGI_COLOR_SPACE_TYPE cs = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  if (supported_hdr10 && hdr) {
    switch (format_) {
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        // The application creates the HDR10 signal.
        cs = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        break;
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        // The system creates the HDR10 signal; application uses linear values.
        cs = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
        break;
      default:
        break;
    }
  }

  UINT supported_cs = 0;
  if (SUCCEEDED(swapchain_->CheckColorSpaceSupport(cs, &supported_cs))) {
    if (supported_cs & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) {
      ThrowIfFailed(swapchain_->SetColorSpace1(cs));
    }
  }

  colorspace_ = cs;
  hdr_ = supported_hdr10;
}

}  // namespace chaos
