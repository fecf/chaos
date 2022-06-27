#include "common.hlsli"

ConstantBuffer<RootConst> Constants : register(b0);

struct VSInput {
  float2 pos : POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

struct VSOutput {
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

VSOutput main(VSInput input) {
  VSOutput output;
  output.pos = mul(Constants.ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
  output.col = input.col;
  output.uv = input.uv;

  if (Constants.DstPixelOffset.x != 0.0f || Constants.DstPixelOffset.y != 0.0f) {
    output.pos.x += Constants.DstPixelOffset.x;
    output.pos.y += Constants.DstPixelOffset.y;
    output.col = float4(0.0f, 0.0f, 0.0f, 1.0f);
  }
  return output;
};
