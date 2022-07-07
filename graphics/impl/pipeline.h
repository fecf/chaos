#pragma once

#include <memory>

#include "d3d12def.h"

namespace chaos {

class Shader;

class PipelineConfig {
 public:
  static PipelineConfig Create(DXGI_FORMAT format,
      ID3D12RootSignature* root_signature, Shader* vertex_shader,
      Shader* pixel_shader);
  static PipelineConfig CreateImGui(DXGI_FORMAT format,
      ID3D12RootSignature* root_signature, Shader* vertex_shader,
      Shader* pixel_shader);

  ID3D12RootSignature* root_signature;
  Shader* vertex_shader;
  Shader* pixel_shader;
  DXGI_FORMAT rtv_format[8];
  DXGI_FORMAT dsv_format;
  std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
  D3D12_BLEND_DESC blend_state;
  D3D12_RASTERIZER_DESC rasterizer_state;
  D3D12_DEPTH_STENCIL_DESC depth_stencil_state;
};

class Pipeline {
 public:
  Pipeline(ComPtr<ID3D12Device> device, const PipelineConfig& config);
  ~Pipeline();

  operator ComPtr<ID3D12PipelineState>() const { return pipeline; }
  operator ID3D12PipelineState*() const { return pipeline.Get(); }

 protected:
  ComPtr<ID3D12PipelineState> pipeline;
};

}  // namespace chaos
