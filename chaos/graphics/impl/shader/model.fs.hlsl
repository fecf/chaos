#include "common.hlsli"

ConstantBuffer<RootConst> Constants : register(b0);
Texture2D Texture : register(t0);
SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VSOutput In) : SV_TARGET {
  float4 col = Texture.Sample(PointSampler, In.UV);
	return col;
}