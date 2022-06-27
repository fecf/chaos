#include "context.h"

static constexpr size_t kMaxBackBufferCount = 3;
static constexpr D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_0;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include <cassert>
#include <stdexcept>
#include <system_error>

#include "pipeline.h"
#include "resource.h"
#include "base/types.h"
#include "base/minlog.h"

// shader
#include "shader.h"
#include "shader/common.hlsli"
#include "shader/compiled/model.fs.h"
#include "shader/compiled/model.vs.h"

// imgui
#include "graphics/imguidef.h"
#include "shader/compiled/imgui.fs.h"
#include "shader/compiled/imgui.vs.h"

bool ImGui_ImplWin32_Init(void* hwnd);

#define SET_ROOT_CONSTANTS(cmdlist, type, name, value)                        \
  cmdlist->SetGraphicsRoot32BitConstants(ROOT_DESCRIPTOR_ROOT_CONSTANTS_SLOT, \
      sizeof(type::name) / 4, value, offsetof(type, name) / 4);

namespace chaos {

inline DXGI_FORMAT noSRGB(DXGI_FORMAT fmt) noexcept {
  switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8X8_UNORM;
    default:
      return fmt;
  }
}

Context::Context() noexcept(false)
    : backbufferindex_(0),
      backbufferformat_(DXGI_FORMAT_R16G16B16A16_FLOAT),
      depthbufferformat_(DXGI_FORMAT_D32_FLOAT),
      backbuffercount_(kMaxBackBufferCount),
      hwnd_(nullptr),
      dxgifactoryflags_(0),
      output_size_{0, 0, 1, 1}
{
  assert(backbuffercount_ > 1 && backbuffercount_ <= kMaxBackBufferCount);
#if defined(_DEBUG)
  dxgifactoryflags_ = DXGI_CREATE_FACTORY_DEBUG;
#endif
  CreateDeviceResources();
  CreateImGuiResources();
}

Context::~Context() { 
  copy_queue_->Wait(0);
  render_queue_->Wait(backbufferindex_);
}

void Context::CreateDeviceResources() {
#if defined(_DEBUG)
  ThrowIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(debug_.GetAddressOf())));
  debug_->EnableDebugLayer();

  ThrowIfFailed(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgidebug_)));
  dxgidebug_->EnableLeakTrackingForThread();

  ComPtr<IDXGIInfoQueue> dxgiinfoqueue;
  ThrowIfFailed(
      ::DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiinfoqueue.GetAddressOf())));
  ThrowIfFailed(dxgiinfoqueue->SetBreakOnSeverity(
      DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
  ThrowIfFailed(dxgiinfoqueue->SetBreakOnSeverity(
      DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));

  /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does
         not control the output on which the swapchain's window resides. */
  DXGI_INFO_QUEUE_MESSAGE_ID hide[]{80};
  DXGI_INFO_QUEUE_FILTER filter{};
  filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
  filter.DenyList.pIDList = hide;
  dxgiinfoqueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
#endif

  ThrowIfFailed(::CreateDXGIFactory2(
      dxgifactoryflags_, IID_PPV_ARGS(dxgifactory_.ReleaseAndGetAddressOf())));

  ComPtr<IDXGIAdapter> adapter;
  {
    ComPtr<IDXGIFactory6> factory6;
    ThrowIfFailed(dxgifactory_.As(&factory6));

    const auto test_adapter = [](int index,
                                  ComPtr<IDXGIAdapter1> adapter) -> bool {
      DXGI_ADAPTER_DESC1 desc{};
      ThrowIfFailed(adapter->GetDesc1(&desc));
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        return false;
      }
      HRESULT hr = ::D3D12CreateDevice(
          adapter.Get(), kMinFeatureLevel, _uuidof(ID3D12Device), nullptr);
      return SUCCEEDED(hr);
    };

    ComPtr<IDXGIAdapter1> ret;
    for (int i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
             i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&ret)));
         ++i) {
      if (test_adapter(i, ret)) {
        break;
      }
    }

    if (!ret) {
      for (UINT i = 0; SUCCEEDED(
               dxgifactory_->EnumAdapters1(i, ret.ReleaseAndGetAddressOf()));
           ++i) {
        if (test_adapter(i, ret)) {
          break;
        }
      }
    }

#if !defined(NDEBUG)
    if (!ret) {
      // Try WARP12 instead
      if (FAILED(dxgifactory_->EnumWarpAdapter(
              IID_PPV_ARGS(ret.ReleaseAndGetAddressOf())))) {
        throw std::runtime_error(
            "WARP12 not available. Enable the 'Graphics Tools' optional "
            "feature");
      }
    }
#endif

    if (!ret) {
      throw std::runtime_error("No Direct3D 12 device found");
    }
    adapter = ret;
  }

  ComPtr<ID3D12Device> device;
  ThrowIfFailed(::D3D12CreateDevice(adapter.Get(), kMinFeatureLevel,
      IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));
  ThrowIfFailed(device->SetName(L"Device"));

  HRESULT hr = device.As(&d3d_);
  ThrowIfFailed(hr);

  ComPtr<ID3D12InfoQueue> d3dInfoQueue;
  if (SUCCEEDED(d3d_.As(&d3dInfoQueue))) {
#ifdef _DEBUG
    d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
    D3D12_MESSAGE_ID hide[] = {
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
    };
    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
    filter.DenyList.pIDList = hide;
    d3dInfoQueue->AddStorageFilterEntries(&filter);
  }

  D3D12_FEATURE_DATA_FEATURE_LEVELS fdfl;
  fdfl.MaxSupportedFeatureLevel = kMinFeatureLevel;
  fdfl.NumFeatureLevels = 1;
  fdfl.pFeatureLevelsRequested = &kMinFeatureLevel;
  ThrowIfFailed(d3d_->CheckFeatureSupport(
      D3D12_FEATURE_FEATURE_LEVELS, &fdfl, sizeof(fdfl)));

  // Create descriptor heaps for render target views and depth stencil views.
  rtv_heap_ = std::unique_ptr<DescriptorHeap>(
      new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, backbuffercount_,
          backbuffercount_, false));
  dsv_heap_ = std::unique_ptr<DescriptorHeap>(
      new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 1, false));
  sampler_heap_ = std::unique_ptr<DescriptorHeap>(
      new DescriptorHeap(d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2, 2, true));
  srv_uav_heap_ = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(
      d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16, 16, false));
  circular_heap_ =
      std::unique_ptr<CircularDescriptorHeap>(new CircularDescriptorHeap(
          d3d_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, backbuffercount_));

  // Create command queue
  render_queue_ = std::unique_ptr<CommandQueue>(new CommandQueue(
      d3d_.Get(), CommandQueue::CommandListType::Direct, kMaxBackBufferCount));
  copy_queue_ = std::unique_ptr<CommandQueue>(
      new CommandQueue(d3d_.Get(), CommandQueue::CommandListType::Copy, 1));

  // Allocator
  D3D12MA::ALLOCATOR_DESC desc{};
  desc.pDevice = d3d_.Get();
  desc.pAdapter = adapter.Get();
  desc.PreferredBlockSize = 1024 * 1024;
  ThrowIfFailed(D3D12MA::CreateAllocator(&desc, &allocator_));

  // Resource Manager
  resource_gc_ = std::unique_ptr<ResourceGC>(new ResourceGC());

  // Shader
  D3D12_FEATURE_DATA_SHADER_MODEL sm = {D3D_SHADER_MODEL_6_0};
  if (FAILED(d3d_->CheckFeatureSupport(
          D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm))) ||
      (sm.HighestShaderModel < D3D_SHADER_MODEL_6_0)) {
    throw std::runtime_error("Shader Model 6.0 is not supported!");
  }
  vs_ = CreateShaderFromBytecode(model_vs, sizeof(model_vs));
  fs_ = CreateShaderFromBytecode(model_fs, sizeof(model_fs));

  // Root Descriptor
  constexpr int size = sizeof(RootConst) / 4;
  CD3DX12_ROOT_PARAMETER root_params[ROOT_DESCRIPTOR_PARAMS];
  root_params[ROOT_DESCRIPTOR_ROOT_CONSTANTS_SLOT].InitAsConstants(
      size, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_DESCRIPTOR_RANGE vs_cbv(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  CD3DX12_DESCRIPTOR_RANGE ps_cbv(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  CD3DX12_DESCRIPTOR_RANGE srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE sampler(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);
  root_params[ROOT_DESCRIPTOR_VS_CBV_SLOT].InitAsDescriptorTable(
      1, &vs_cbv, D3D12_SHADER_VISIBILITY_VERTEX);
  root_params[ROOT_DESCRIPTOR_PS_CBV_SLOT].InitAsDescriptorTable(
      1, &ps_cbv, D3D12_SHADER_VISIBILITY_PIXEL);
  root_params[ROOT_DESCRIPTOR_PS_SRV_SLOT].InitAsDescriptorTable(
      1, &srv, D3D12_SHADER_VISIBILITY_PIXEL);
  root_params[ROOT_DESCRIPTOR_PS_SAMPLER_SLOT].InitAsDescriptorTable(
      1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);
  auto flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
               D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
               D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
               D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> err;
  {
    CD3DX12_ROOT_SIGNATURE_DESC desc(
        _countof(root_params), root_params, 0, nullptr, flags);
    hr = ::D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &err);
  }
  if (FAILED(hr)) {
    const char* msg = (const char*)err->GetBufferPointer();
    std::string str = "failed to D3D12SerializeRootSignature(). ";
    str += msg;
    throw std::runtime_error(str.c_str());
  }
  hr = d3d_->CreateRootSignature(0, signature->GetBufferPointer(),
      signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
  if (FAILED(hr)) {
    throw std::runtime_error("failed to CreateRootSignature().");
  }

  // Sampler (shared with imgui)
  {
    D3D12_SAMPLER_DESC desc{};
    DescriptorHandle handle;

    desc.Filter = D3D12_FILTER::D3D12_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    handle = sampler_heap_->GetDescriptorHandle(0);
    d3d_->CreateSampler(&desc, handle.cpu);

    desc.Filter = D3D12_FILTER::D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    handle = sampler_heap_->GetDescriptorHandle(1);
    d3d_->CreateSampler(&desc, handle.cpu);
  }

  // Pipeline
  PipelineConfig pipeline_config = PipelineConfig::Create(
      backbufferformat_, root_signature_.Get(), vs_.get(), fs_.get());
  pipeline_ = std::unique_ptr<Pipeline>(new Pipeline(d3d_, pipeline_config));

  // Constant Buffer
  vs_cbv_.resize(backbuffercount_);
  for (int i = 0; i < backbuffercount_; ++i) {
    vs_cbv_[i] = CreateResource(
        ResourceDesc(ResourceUsage::Constant, sizeof(VsCBVDesc) + 255 & ~255));
  }
  ps_cbv_.resize(backbuffercount_);
  for (int i = 0; i < backbuffercount_; ++i) {
    ps_cbv_[i] = CreateResource(
        ResourceDesc(ResourceUsage::Constant, sizeof(PsCBVDesc) + 255 & ~255));
  }
}

void Context::CreateImGuiResources() {
  // ImGui
  if (!ImGui::GetCurrentContext()) {
    imgui_context_ = ImGui::CreateContext();
  }

  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");
  io.BackendRendererUserData = NULL;
  io.BackendRendererName = "imgui_impl_dx12";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  imgui_resources_.resize(backbuffercount_);
  for (int i = 0; i < imgui_resources_.size(); i++) {
    imgui_resources_[i].ib_.reset();
    imgui_resources_[i].vb_.reset();
    imgui_resources_[i].ib_size_ = 10000;
    imgui_resources_[i].vb_size_ = 5000;
  }

  imgui_vs_ = CreateShaderFromBytecode(imgui_vs, sizeof(imgui_vs));
  imgui_fs_ = CreateShaderFromBytecode(imgui_fs, sizeof(imgui_fs));
  PipelineConfig imgui_pipeline_config =
      PipelineConfig::CreateImGui(backbufferformat_, root_signature_.Get(),
          imgui_vs_.get(), imgui_fs_.get());
  imgui_pipeline_ =
      std::unique_ptr<Pipeline>(new Pipeline(d3d_, imgui_pipeline_config));
}

void Context::CreateWindowSizeDependentResources() {
  if (!hwnd_) {
    throw std::runtime_error("hwnd is null.");
  }

  // WaitForLastSubmittedFrame
  uint64_t fence_value = render_queue_->GetFenceValue(backbufferindex_);
  if (fence_value > 0) {
    render_queue_->SetFenceValue(backbufferindex_, 0);
    render_queue_->WaitOnCompletion(backbufferindex_, fence_value);
  }

  const UINT width = std::max<UINT>(static_cast<UINT>(output_size_.right - output_size_.left), 1u);
  const UINT height = std::max<UINT>(static_cast<UINT>(output_size_.bottom - output_size_.top), 1u);
  const DXGI_FORMAT format = noSRGB(backbufferformat_);
  if (swapchain_) {
    HRESULT hr = swapchain_->Resize(backbuffercount_, width, height, format);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      handleDeviceLost(hr);
      return;
    } else {
      ThrowIfFailed(hr);
    }
    backbufferindex_ = swapchain_->GetCurrentBackBufferIndex();
  } else {
    swapchain_ = std::unique_ptr<SwapChain>(
        new SwapChain(dxgifactory_, d3d_, render_queue_->GetCommandQueue(),
            hwnd_, width, height, format, backbuffercount_));
    backbufferindex_ = swapchain_->GetCurrentBackBufferIndex();
  }

  // CreateRenderTarget
  for (int i = 0; i < backbuffercount_; ++i) {
    D3D12_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = format;
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    DescriptorHandle descriptor_handle = rtv_heap_->GetDescriptorHandle(i);
    ComPtr<ID3D12Resource> rt = swapchain_->GetRenderTarget(i).Get();
    d3d_->CreateRenderTargetView(rt.Get(), &desc, descriptor_handle.cpu);
  }

  if (depthbufferformat_ != DXGI_FORMAT_UNKNOWN) {
    CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC depthStencilDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(depthbufferformat_, width, height, 1, 1);
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE depthOptimizedClearValue{};
    depthOptimizedClearValue.Format = depthbufferformat_;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;
    ThrowIfFailed(d3d_->CreateCommittedResource(&depthHeapProperties,
        D3D12_HEAP_FLAG_NONE, &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue,
        IID_PPV_ARGS(depthstencil_.ReleaseAndGetAddressOf())));
    depthstencil_->SetName(L"DepthStencil");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = depthbufferformat_;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    DescriptorHandle descriptor_handle = dsv_heap_->GetDescriptorHandle(0);
    d3d_->CreateDepthStencilView(
        depthstencil_.Get(), &dsvDesc, descriptor_handle.cpu);
  }
}

void Context::SetWindow(HWND window, int width, int height) {
  assert(hwnd_ == NULL);
  hwnd_ = window;
  output_size_.left = output_size_.top = 0;
  output_size_.right = width;
  output_size_.bottom = height;
  CreateWindowSizeDependentResources();
}

bool Context::WindowSizeChanged(int width, int height) {
  RECT rc;
  rc.left = rc.top = 0;
  rc.right = width;
  rc.bottom = height;
  output_size_ = rc;
  CreateWindowSizeDependentResources();
  return true;
}

void Context::Prepare() {
  // Wait swapchain
  swapchain_->Wait();

  // WaitForGpu
  render_queue_->Wait(backbufferindex_);

  uint64_t fence_value = render_queue_->GetFenceValue(backbufferindex_);
  GetResourceGC()->Cleanup(fence_value);

  ComPtr<ID3D12CommandAllocator> cmdalloc = render_queue_->GetCommandAllocator(backbufferindex_);
  ComPtr<ID3D12GraphicsCommandList> cmdlist = render_queue_->GetCommandList();
  ComPtr<ID3D12CommandQueue> cmdqueue = render_queue_->GetCommandQueue();
  ThrowIfFailed(cmdalloc->Reset());
  ThrowIfFailed(cmdlist->Reset(cmdalloc.Get(), nullptr));

  ComPtr<ID3D12Resource> render_target = swapchain_->GetRenderTarget(backbufferindex_);
  D3D12_RESOURCE_BARRIER barrier_before =
      CD3DX12_RESOURCE_BARRIER::Transition(render_target.Get(),
          D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  cmdlist->ResourceBarrier(1, &barrier_before);
}

void Context::Render() {
  ComPtr<ID3D12GraphicsCommandList> cmdlist = render_queue_->GetCommandList();
  ComPtr<ID3D12CommandQueue> cmdqueue = render_queue_->GetCommandQueue();

  auto rtv_descriptor_handle = rtv_heap_->GetDescriptorHandle(backbufferindex_);
  const FLOAT color[4]{ 0.0, 0.5, 0.5, 1.0 };
  cmdlist->ClearRenderTargetView(rtv_descriptor_handle.cpu, color, 0, nullptr);

  renderImGui(cmdlist, root_signature_, *imgui_pipeline_);

  ComPtr<ID3D12Resource> render_target = swapchain_->GetRenderTarget(backbufferindex_);
  D3D12_RESOURCE_BARRIER barrier_after =
      CD3DX12_RESOURCE_BARRIER::Transition(render_target.Get(),
          D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  cmdlist->ResourceBarrier(1, &barrier_after);

  ThrowIfFailed(cmdlist->Close());
  cmdqueue->ExecuteCommandLists(1, CommandListCast(cmdlist.GetAddressOf()));

  HRESULT hr = swapchain_->Present();
  if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
    handleDeviceLost(hr);
  } else {
    ThrowIfFailed(hr);

    // MoveToNextFrame
    backbufferindex_ = swapchain_->GetCurrentBackBufferIndex();
    render_queue_->Signal(backbufferindex_);
    uint64_t completed_value = render_queue_->GetFence()->GetCompletedValue();
    uint64_t fence_value = render_queue_->GetFenceValue(backbufferindex_);
    if (completed_value < fence_value) {
      render_queue_->WaitOnCompletion(backbufferindex_, fence_value);
    }
    render_queue_->SetFenceValue(backbufferindex_, fence_value + 1);

    if (!dxgifactory_->IsCurrent()) {
      ThrowIfFailed(::CreateDXGIFactory2(dxgifactoryflags_,
          IID_PPV_ARGS(dxgifactory_.ReleaseAndGetAddressOf())));
    }
  }
}

void Context::handleDeviceLost(HRESULT hr) {
#ifdef _DEBUG
    char buff[64]{};
    sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
        static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED)
                                      ? d3d_->GetDeviceRemovedReason()
                                      : hr));
    OutputDebugStringA(buff);
#endif
  notifyDeviceLost();

  swapchain_.reset();
  resource_gc_.reset();
  render_queue_.reset();
  depthstencil_.Reset();
  rtv_heap_.reset();
  dsv_heap_.reset();
  sampler_heap_.reset();
  srv_uav_heap_.reset();
  circular_heap_.reset();
  allocator_.Reset();
  d3d_.Reset();
  dxgifactory_.Reset();

#ifdef _DEBUG
  ThrowIfFailed(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgidebug_)));
  ThrowIfFailed(dxgidebug_->ReportLiveObjects(
      DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL)));
#endif

  CreateDeviceResources();
  CreateImGuiResources();
  CreateWindowSizeDependentResources();

  notifyDeviceRestored();
}

void Context::notifyDeviceLost() {}

void Context::notifyDeviceRestored() {}

void Context::prepareScene() {}

void Context::renderScene() {}

void Context::renderImGui(ComPtr<ID3D12GraphicsCommandList> ctx,
    ComPtr<ID3D12RootSignature> root_signature, ComPtr<ID3D12PipelineState> pipeline) {
  ctx->SetGraphicsRootSignature(root_signature.Get());
  ctx->SetPipelineState(pipeline.Get());

  auto rtv_descriptor_handle = rtv_heap_->GetDescriptorHandle(backbufferindex_);
  const FLOAT color[4]{};
  ctx->ClearRenderTargetView(rtv_descriptor_handle.cpu, color, 0, nullptr);

  auto dsv_descriptor_handle = dsv_heap_->GetDescriptorHandle(0);
  ctx->OMSetRenderTargets(1, &rtv_descriptor_handle.cpu, FALSE, &dsv_descriptor_handle.cpu);
  ctx->ClearDepthStencilView(
      dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  RECT rect{};
  ::GetClientRect(hwnd_, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
  CD3DX12_VIEWPORT vp(0.0f, 0.0f, (FLOAT)width, (FLOAT)height);
  CD3DX12_RECT scissor(0, 0, width, height);
  ctx->RSSetViewports(1, &vp);
  ctx->RSSetScissorRects(1, &scissor);

  circular_heap_->Upload(backbufferindex_, srv_uav_heap_.get());

  std::array<ID3D12DescriptorHeap*, 2> heaps;
  heaps[0] = circular_heap_->GetHeap(backbufferindex_)->GetHeap();
  heaps[1] = sampler_heap_->GetHeap();
  ctx->SetDescriptorHeaps((int)heaps.size(), heaps.data());

  // sampler
  ctx->SetGraphicsRootDescriptorTable(ROOT_DESCRIPTOR_PS_SAMPLER_SLOT,
      sampler_heap_->GetDescriptorHandle(0).gpu);

  // Render ImGUI
  ctx->SetPipelineState(*imgui_pipeline_);

  ImDrawData* dd = ImGui::GetDrawData();
  int imgui_draw_call_count = 0;
  if (dd != nullptr) {
    auto resetRenderState = [&](ImDrawData* dd, ID3D12GraphicsCommandList* ctx,
                              ImGuiResource* fr) {
      ctx->SetGraphicsRootSignature(root_signature_.Get());
      ctx->SetPipelineState(*imgui_pipeline_);

      const float L = dd->DisplayPos.x;
      const float R = dd->DisplayPos.x + dd->DisplaySize.x;
      const float T = dd->DisplayPos.y;
      const float B = dd->DisplayPos.y + dd->DisplaySize.y;
      const float mvp[4][4]{
          {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
          {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
          {0.0f, 0.0f, 0.5f, 0.0f},
          {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
      };
      SET_ROOT_CONSTANTS(ctx, RootConst, ProjectionMatrix, &mvp);

      CD3DX12_VIEWPORT vp(0.0f, 0.0f, dd->DisplaySize.x, dd->DisplaySize.y);
      ctx->RSSetViewports(1, &vp);

      D3D12_VERTEX_BUFFER_VIEW vbv{};
      vbv.BufferLocation = fr->vb_->GetResource()->GetGPUVirtualAddress();
      vbv.SizeInBytes = fr->vb_size_ * sizeof(ImDrawVert);
      vbv.StrideInBytes = sizeof(ImDrawVert);
      ctx->IASetVertexBuffers(0, 1, &vbv);
      D3D12_INDEX_BUFFER_VIEW ibv{};
      ibv.BufferLocation = fr->ib_->GetResource()->GetGPUVirtualAddress();
      ibv.SizeInBytes = fr->ib_size_ * sizeof(ImDrawIdx);
      ibv.Format =
          sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
      ctx->IASetIndexBuffer(&ibv);
      ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      const float blend_factor[4]{};
      ctx->OMSetBlendFactor(blend_factor);
    };

    // Avoid rendering when minimized
    if (dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) {
      return;
    }

    // Create and grow vertex/index buffers if needed
    ImGuiResource* fr = &imgui_resources_[backbufferindex_];
    if (!fr->vb_ || fr->vb_size_ < dd->TotalVtxCount) {
      fr->vb_size_ = dd->TotalVtxCount + 5000;
      fr->vb_ = CreateResource(ResourceDesc(
          ResourceUsage::Vertex, fr->vb_size_ * sizeof(ImDrawVert)));
    }

    if (!fr->ib_ || fr->ib_size_ < dd->TotalIdxCount) {
      fr->ib_size_ = dd->TotalIdxCount + 5000;
      fr->ib_ = CreateResource(
          ResourceDesc(ResourceUsage::Index, fr->ib_size_ * sizeof(ImDrawIdx)));
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    ImDrawVert* vtx_dst = (ImDrawVert*)fr->vb_->MapStaging();
    ImDrawIdx* idx_dst = (ImDrawIdx*)fr->ib_->MapStaging();
    for (int n = 0; n < dd->CmdListsCount; n++) {
      const ImDrawList* drawlist = dd->CmdLists[n];
      ::memcpy(vtx_dst, drawlist->VtxBuffer.Data,
          drawlist->VtxBuffer.Size * sizeof(ImDrawVert));
      ::memcpy(idx_dst, drawlist->IdxBuffer.Data,
          drawlist->IdxBuffer.Size * sizeof(ImDrawIdx));
      vtx_dst += drawlist->VtxBuffer.Size;
      idx_dst += drawlist->IdxBuffer.Size;
    }
    fr->vb_->UnmapStaging();
    fr->ib_->UnmapStaging();
    ctx->CopyBufferRegion(fr->vb_->GetResource(), 0,
        fr->vb_->GetStagingResource(), 0, fr->vb_->GetDesc().size);
    ctx->CopyBufferRegion(fr->ib_->GetResource(), 0,
        fr->ib_->GetStagingResource(), 0, fr->ib_->GetDesc().size);
    D3D12_RESOURCE_BARRIER barriers[2]{
        CD3DX12_RESOURCE_BARRIER::Transition(fr->ib_->GetResource(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER),
        CD3DX12_RESOURCE_BARRIER::Transition(fr->vb_->GetResource(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
    };
    ctx->ResourceBarrier(2, barriers);

    // Setup desired DX state
    resetRenderState(dd, ctx.Get(), fr);

    int vtx_offset = 0;
    int idx_offset = 0;
    bool outline = false;
    ColorSpace src_color_space = ColorSpace::sRGB;
    ColorSpace dst_color_space = (backbufferformat_ == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ColorSpace::Linear : ColorSpace::sRGB;
    TextureFilter filter = TextureFilter::Bilinear;
    SET_ROOT_CONSTANTS(ctx, RootConst, SrcColorSpace, &src_color_space);
    SET_ROOT_CONSTANTS(ctx, RootConst, DstColorSpace, &dst_color_space);
    SET_ROOT_CONSTANTS(ctx, RootConst, TextureFilter, &filter);

    for (int i = 0; i < dd->CmdListsCount; i++) {
      const ImDrawList* cmd_list = dd->CmdLists[i];
      for (int j = 0; j < cmd_list->CmdBuffer.Size; j++) {
        const ImDrawCmd& cmd = cmd_list->CmdBuffer[j];

        // callback
        if (cmd.UserCallback != NULL) {
          if (cmd.UserCallback == ImDrawCallback_ResetRenderState) {
            resetRenderState(dd, ctx.Get(), fr);
          } else if (cmd.UserCallback == (void*)kImDrawCallbackSetDrawOutline) {
            outline = reinterpret_cast<uint64_t>(cmd.UserCallbackData) > 0;
          } else if (cmd.UserCallback ==
                     (void*)kImDrawCallbackSetTextureFilter) {
            filter = (TextureFilter) reinterpret_cast<uint64_t>(
                cmd.UserCallbackData);
            SET_ROOT_CONSTANTS(ctx, RootConst, TextureFilter, &filter);
          } else if (cmd.UserCallback ==
                     (void*)kImDrawCallbackSetSrcColorSpace) {
            src_color_space =
                (ColorSpace) reinterpret_cast<uint64_t>(cmd.UserCallbackData);
            SET_ROOT_CONSTANTS(ctx, RootConst, SrcColorSpace, &src_color_space);
          } else if (cmd.UserCallback ==
                     (void*)kImDrawCallbackSetDstColorSpace) {
            dst_color_space =
                (ColorSpace) reinterpret_cast<uint64_t>(cmd.UserCallbackData);
            SET_ROOT_CONSTANTS(ctx, RootConst, DstColorSpace, &dst_color_space);
          }
          continue;
        }

        const ImVec2 clip_off = dd->DisplayPos;
        ImVec2 clip_min(
            cmd.ClipRect.x - clip_off.x, cmd.ClipRect.y - clip_off.y);
        ImVec2 clip_max(
            cmd.ClipRect.z - clip_off.x, cmd.ClipRect.w - clip_off.y);
        if ((LONG)clip_max.x <= (LONG)clip_min.x ||
            (LONG)clip_max.y <= (LONG)clip_min.y) {
          continue;
        }

        const D3D12_RECT r = {(LONG)clip_min.x, (LONG)clip_min.y,
            (LONG)clip_max.x, (LONG)clip_max.y};
        ctx->RSSetScissorRects(1, &r);

        int slot = static_cast<int>(reinterpret_cast<uint64_t>(cmd.GetTexID()));
        DescriptorHandle handle =
            circular_heap_->GetDescriptorHandle(backbufferindex_, slot);
        ctx->SetGraphicsRootDescriptorTable(
            ROOT_DESCRIPTOR_PS_SRV_SLOT, handle.gpu);

        if (outline) {
          const float dist = 2.0f;
          const linalg::aliases::float2 offset[4]{
              {-dist / width, 0.0f},
              {+dist / width, 0.0f},
              {0.0f, -dist / height},
              {0.0f, +dist / height},
          };
          for (int k = 0; k < 4; ++k) {
            SET_ROOT_CONSTANTS(ctx, RootConst, DstPixelOffset, &offset[k]);
            ctx->DrawIndexedInstanced(cmd.ElemCount, 1,
                cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);
            imgui_draw_call_count++;
          }
          linalg::aliases::float2 base = {0.0f, 0.0f};
          SET_ROOT_CONSTANTS(ctx, RootConst, DstPixelOffset, &base);
        }

        ctx->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset,
            cmd.VtxOffset + vtx_offset, 0);
        imgui_draw_call_count++;
      }
      idx_offset += cmd_list->IdxBuffer.Size;
      vtx_offset += cmd_list->VtxBuffer.Size;
    }
  }
  imgui_draw_call_count_ = imgui_draw_call_count;
}

void Context::CopyResource(Resource* resource, const void* data, size_t size) {
  const ResourceDesc& desc = resource->GetDesc();
  assert(desc.usage != ResourceUsage::Image);

  void* mapped = resource->MapStaging();
  assert(mapped != NULL);
  ::memcpy(mapped, data, size);
  resource->UnmapStaging();

  CopyResourceFromStaging(resource);
}

void Context::CopyResource2D(
    Resource* resource, const void* data, size_t pitch, int height) {
  const ResourceDesc& desc = resource->GetDesc();
  assert(desc.usage == ResourceUsage::Image);

  void* mapped = resource->MapStaging();
  assert(mapped != NULL);
  if (desc.pitch == pitch) {
    ::memcpy(mapped, data, pitch * height);
  } else {
    for (int y = 0; y < height; ++y) {
      ::memcpy((uint8_t*)mapped + y * desc.pitch,
          (const uint8_t*)data + y * pitch, pitch);
    }
  }
  resource->UnmapStaging();

  CopyResourceFromStaging(resource);
}

void Context::CopyResourceFromStaging(Resource* resource) {
  std::lock_guard lock(mutex_staging_);

  const ResourceDesc& desc = resource->GetDesc();

  ComPtr<ID3D12GraphicsCommandList> cmdlist = copy_queue_->GetCommandList();
  ThrowIfFailed(cmdlist->Reset(copy_queue_->GetCommandAllocator(0).Get(), NULL));

  D3D12_RESOURCE_BARRIER barrier0 =
      CD3DX12_RESOURCE_BARRIER::Transition(resource->GetResource(),
          D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
  cmdlist->ResourceBarrier(1, &barrier0);

  if (desc.usage == ResourceUsage::Image) {
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    footprint.Footprint = CD3DX12_SUBRESOURCE_FOOTPRINT(
        desc.format, desc.width, desc.height, 1, (int)desc.pitch);
    CD3DX12_TEXTURE_COPY_LOCATION src(
        resource->GetStagingResource(), footprint);
    CD3DX12_TEXTURE_COPY_LOCATION dst(resource->GetResource());
    cmdlist->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
  } else {
    cmdlist->CopyBufferRegion(resource->GetResource(), 0,
        resource->GetStagingResource(), 0, desc.size);
  }

  D3D12_RESOURCE_BARRIER barrier1 =
      CD3DX12_RESOURCE_BARRIER::Transition(resource->GetResource(),
          D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
  cmdlist->ResourceBarrier(1, &barrier1);

  ThrowIfFailed(cmdlist->Close());
  ThrowIfFailed(copy_queue_->GetFence()->Signal(0));
  copy_queue_->GetCommandQueue()->ExecuteCommandLists(
      1, (ID3D12CommandList**)cmdlist.GetAddressOf());
  ThrowIfFailed(
      copy_queue_->GetCommandQueue()->Signal(copy_queue_->GetFence().Get(), 1));
  copy_queue_->WaitOnCompletion(0, 1);
  ThrowIfFailed(copy_queue_->GetCommandAllocator(0)->Reset());
}
std::unique_ptr<Resource> Context::CreateResource(const ResourceDesc& desc) {
  D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  if (desc.usage == ResourceUsage::Constant) {
    heap_type = D3D12_HEAP_TYPE_UPLOAD;
    state = D3D12_RESOURCE_STATE_GENERIC_READ;
  }

  D3D12MA::ALLOCATION_DESC allocdesc{};
  allocdesc.HeapType = heap_type;
  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
  D3D12_RESOURCE_DESC1 resourcedesc;
  if (desc.usage == ResourceUsage::Image) {
    resourcedesc =
        CD3DX12_RESOURCE_DESC1::Tex2D(desc.format, desc.width, desc.height);
  } else {
    resourcedesc = CD3DX12_RESOURCE_DESC1::Buffer(desc.size, flags);
  }

  // Resource
  ComPtr<D3D12MA::Allocation> allocation;
  ComPtr<ID3D12Resource2> resource;
  ThrowIfFailed(allocator_->CreateResource2(&allocdesc, &resourcedesc, state,
      nullptr, &allocation, IID_PPV_ARGS(&resource)));
  ThrowIfFailed(resource->SetName(L"Resource"));

  // View
  DescriptorHandle descriptor_handle{};
  if (desc.usage == ResourceUsage::Constant) {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
    cbv.BufferLocation = resource->GetGPUVirtualAddress();
    cbv.SizeInBytes = (UINT)desc.size;
    descriptor_handle = srv_uav_heap_->Allocate();
    d3d_->CreateConstantBufferView(&cbv, descriptor_handle.cpu);
  } else if (desc.usage == ResourceUsage::Unordered) {
    descriptor_handle = srv_uav_heap_->Allocate();
    d3d_->CreateUnorderedAccessView(
        resource.Get(), nullptr, nullptr, descriptor_handle.cpu);
  } else if (desc.usage == ResourceUsage::Image) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc{};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = resourcedesc.Format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;
    descriptor_handle = srv_uav_heap_->Allocate();
    d3d_->CreateShaderResourceView(resource.Get(), &srvdesc, descriptor_handle.cpu);
  }

  // Staging Resource
  ComPtr<D3D12MA::Allocation> staging_allocation;
  ComPtr<ID3D12Resource2> staging_resource;
  if (desc.usage != ResourceUsage::Constant) {
    D3D12MA::ALLOCATION_DESC allocdesc{};
    allocdesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    size_t size = desc.size;
    if (desc.usage != ResourceUsage::Image) {
      constexpr int align = 256;
      size = (desc.size + align - 1) & ~(align - 1);
    }
    D3D12_RESOURCE_DESC1 resourcedesc =
        CD3DX12_RESOURCE_DESC1::Buffer(desc.size, flags);

    ThrowIfFailed(allocator_->CreateResource2(&allocdesc, &resourcedesc, state,
        nullptr, &staging_allocation, IID_PPV_ARGS(&staging_resource)));
    ThrowIfFailed(staging_resource->SetName(L"StagingResource"));
  }

  std::unique_ptr<Resource> ret(new Resource(desc));
  ret->resource_ = resource;
  ret->allocation_ = allocation;
  ret->staging_resource_ = staging_resource;
  ret->staging_allocation_ = staging_allocation;
  ret->descriptor_handle_ = descriptor_handle;
  ret->deleter_ = [&](const Resource& resource) {
    const DescriptorHandle& descriptor_handle = resource.GetDescriptorHandle();
    if (descriptor_handle.heap != nullptr) {
      descriptor_handle.heap->Release(descriptor_handle);
    }

    uint64_t fencevalue = render_queue_->GetFenceValue(backbufferindex_);
    resource_gc_->Add(resource, fencevalue + 1);
  };
  return ret;
}

}  // namespace chaos
