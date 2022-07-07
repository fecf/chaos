#include "pipeline.h"

#include "d3d12def.h"
#include <d3dx12.h>

#include "shader.h"

#include "../common.h"
#include "../imgui/imgui.h"

namespace chaos {

PipelineConfig PipelineConfig::Create(DXGI_FORMAT format,
    ID3D12RootSignature* root_signature, Shader* vertex_shader,
    Shader* pixel_shader) {
  PipelineConfig config{};

  config.root_signature = root_signature;
  config.vertex_shader = vertex_shader;
  config.pixel_shader = pixel_shader;

  config.input_layout.resize(3);
  config.input_layout[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
      offsetof(Vertex, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  config.input_layout[1] = {"NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
      offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  config.input_layout[2] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv),
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};

  config.rtv_format[0] = format;
  config.dsv_format = DXGI_FORMAT_D32_FLOAT;

  config.blend_state = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
  config.rasterizer_state = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());  
  config.depth_stencil_state = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());  

  return config;
}

PipelineConfig PipelineConfig::CreateImGui(DXGI_FORMAT format,
    ID3D12RootSignature* root_signature, Shader* vertex_shader,
    Shader* pixel_shader) {
  PipelineConfig config = PipelineConfig::Create(
      format, root_signature, vertex_shader, pixel_shader);

  config.input_layout.resize(3);
  config.input_layout[0] = {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
      offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  config.input_layout[1] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
      offsetof(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  config.input_layout[2] = {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
      offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};

  config.blend_state.AlphaToCoverageEnable = false;
  config.blend_state.RenderTarget[0].BlendEnable = true;
  config.blend_state.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  config.blend_state.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  config.blend_state.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  config.blend_state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  config.blend_state.RenderTarget[0].DestBlendAlpha =
      D3D12_BLEND_INV_SRC_ALPHA;
  config.blend_state.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  config.blend_state.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;

  config.rasterizer_state.FillMode = D3D12_FILL_MODE_SOLID;
  config.rasterizer_state.CullMode = D3D12_CULL_MODE_NONE;
  config.rasterizer_state.FrontCounterClockwise = FALSE;
  config.rasterizer_state.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  config.rasterizer_state.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  config.rasterizer_state.SlopeScaledDepthBias =
      D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  config.rasterizer_state.DepthClipEnable = true;
  config.rasterizer_state.MultisampleEnable = FALSE;
  config.rasterizer_state.AntialiasedLineEnable = FALSE;
  config.rasterizer_state.ForcedSampleCount = 0;
  config.rasterizer_state.ConservativeRaster =
      D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  config.depth_stencil_state.DepthEnable = false;
  config.depth_stencil_state.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  config.depth_stencil_state.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  config.depth_stencil_state.StencilEnable = false;
  config.depth_stencil_state.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
  config.depth_stencil_state.FrontFace.StencilDepthFailOp =
      D3D12_STENCIL_OP_KEEP;
  config.depth_stencil_state.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
  config.depth_stencil_state.FrontFace.StencilFunc =
      D3D12_COMPARISON_FUNC_ALWAYS;
  config.depth_stencil_state.BackFace = config.depth_stencil_state.FrontFace;

  return config;
}

Pipeline::Pipeline(ComPtr<ID3D12Device> device, const PipelineConfig& config) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psodesc{};
  psodesc.pRootSignature = config.root_signature;

  psodesc.VS.pShaderBytecode = config.vertex_shader->GetBytecode();
  psodesc.VS.BytecodeLength = config.vertex_shader->GetSize();
  psodesc.PS.pShaderBytecode = config.pixel_shader->GetBytecode();
  psodesc.PS.BytecodeLength = config.pixel_shader->GetSize();

  psodesc.StreamOutput.pSODeclaration = NULL;
  psodesc.StreamOutput.NumEntries = 0;
  psodesc.StreamOutput.pBufferStrides = NULL;
  psodesc.StreamOutput.NumStrides = 0;
  psodesc.StreamOutput.RasterizedStream = 0;

  psodesc.BlendState = config.blend_state;
  psodesc.SampleMask = UINT_MAX;
  psodesc.RasterizerState = config.rasterizer_state;
  psodesc.DepthStencilState = config.depth_stencil_state;

  psodesc.InputLayout.NumElements = (UINT)config.input_layout.size();
  psodesc.InputLayout.pInputElementDescs = config.input_layout.data();

  psodesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  psodesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psodesc.NumRenderTargets = 1;
  constexpr int size = sizeof(config.rtv_format) / sizeof(config.rtv_format[0]);
  for (int i = 0; i < size; ++i) {
    psodesc.RTVFormats[i] = config.rtv_format[i];
  }
  psodesc.DSVFormat = config.dsv_format;
  psodesc.SampleDesc.Quality = 0;
  psodesc.SampleDesc.Count = 1;
  psodesc.NodeMask = 0;
  psodesc.CachedPSO.pCachedBlob = NULL;
  psodesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psodesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  HRESULT hr = device->CreateGraphicsPipelineState(
      &psodesc, __uuidof(ID3D12PipelineState), &pipeline);
  if (FAILED(hr)) {
    throw std::runtime_error("failed to CreateGraphicsPipelineState().");
  }
}

Pipeline::~Pipeline() = default;

}  // namespace chaos
