#pragma once

#include <memory>
#include <D3D12MemAlloc.h>

#include "commandqueue.h"
#include "descriptorheap.h"
#include "resource.h"
#include "d3d12def.h"
#include "swapchain.h"
#include "pipeline.h"
#include "shader.h"

namespace chaos {

class Context {
 public:
  Context();
  ~Context();

  void CreateDeviceResources();
  void CreateImGuiResources();
  void CreateWindowSizeDependentResources();
  void SetWindow(HWND window, int width, int height);
  bool WindowSizeChanged(int width, int height);

  void Prepare();
  void Render();

  void CopyResource(Resource* resource, const void* data, size_t size);
  void CopyResource2D(Resource* resource, const void* data, size_t pitch, int height);
  void CopyResourceFromStaging(Resource* resource);
  std::unique_ptr<Resource> CreateResource(const ResourceDesc& desc);
  ResourceGC* GetResourceGC() const noexcept { return resource_gc_.get(); }

  void handleDeviceLost(HRESULT hr);
  void notifyDeviceLost();
  void notifyDeviceRestored();

  void prepareScene();
  void renderScene();

  void renderImGui(ComPtr<ID3D12GraphicsCommandList> ctx,
      ComPtr<ID3D12RootSignature> root_signature,
      ComPtr<ID3D12PipelineState> pipeline);

 private:
  std::mutex mutex_staging_;

  ComPtr<IDXGIFactory7> dxgifactory_;
  DWORD dxgifactoryflags_;
  ComPtr<ID3D12Device8> d3d_;
  ComPtr<ID3D12Debug> debug_;
  ComPtr<IDXGIDebug1> dxgidebug_;
  ComPtr<D3D12MA::Allocator> allocator_;
  std::unique_ptr<ResourceGC> resource_gc_;

  std::unique_ptr<CommandQueue> render_queue_;
  std::unique_ptr<CommandQueue> copy_queue_;

  std::unique_ptr<DescriptorHeap> rtv_heap_;
  std::unique_ptr<DescriptorHeap> dsv_heap_;
  std::unique_ptr<DescriptorHeap> sampler_heap_;
  std::unique_ptr<DescriptorHeap> srv_uav_heap_;
  std::unique_ptr<CircularDescriptorHeap> circular_heap_;
  ComPtr<ID3D12Resource> depthstencil_;

  std::unique_ptr<SwapChain> swapchain_;
  ComPtr<ID3D12RootSignature> root_signature_;

  std::unique_ptr<Pipeline> pipeline_;
  std::unique_ptr<Shader> vs_;
  std::unique_ptr<Shader> fs_;
  std::vector<std::unique_ptr<Resource>> vs_cbv_;
  std::vector<std::unique_ptr<Resource>> ps_cbv_;

  std::unique_ptr<Pipeline> imgui_pipeline_;
  std::unique_ptr<Shader> imgui_vs_;
  std::unique_ptr<Shader> imgui_fs_;
  struct ImGuiResource {
    std::unique_ptr<Resource> ib_;
    std::unique_ptr<Resource> vb_;
    int ib_size_ = 0;
    int vb_size_ = 0;
  };
  std::vector<ImGuiResource> imgui_resources_;
  std::atomic<int> imgui_draw_call_count_;
  ImGuiContext* imgui_context_;

  HWND hwnd_;
  DXGI_FORMAT backbufferformat_;
  DXGI_FORMAT depthbufferformat_;
  int backbuffercount_;
  int backbufferindex_;
  RECT output_size_;
};

}  // namespace chaos
