#include "common.hlsli"

ConstantBuffer<RootConst> Constants : register(b0);

struct VSInput {
  float4 Position : POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD0;
};

struct VSOutput {
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

VSOutput main(VSInput input) {
  VSOutput result;
  result.Position = mul(input.Position, Constants.ProjectionMatrix);
  result.UV = input.UV;
  return result;
}