#include "common.hlsli"

ConstantBuffer<RootConst> Constants : register(b0);
Texture2D Texture : register(t0);
SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

float3 SRGBToLinear(float3 color) {
  // Approximately pow(color, 2.2)
  return color < 0.04045 ? color / 12.92 : pow(abs(color + 0.055) / 1.055, 2.4);
}

float3 LinearToSRGB(float3 color) {
  // Approximately pow(color, 1.0 / .2)
  return color < 0.0031308 ? 12.92 * color
                           : 1.055 * pow(abs(color), 1.0 / 2.4) - 0.055;
}

struct VSOutput {
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target {
  float4 col = input.col;

  // Colors from ImGui is sRGB space.
  if (Constants.SrcColorSpace == COLOR_SPACE_SRGB) {
    col = float4(SRGBToLinear(col.rgb), col.w);
  }

  // Texture
  float4 tex;
  if (Constants.TextureFilter == FILTER_NEAREST) {
    tex = Texture.Sample(PointSampler, input.uv);
    tex = clamp(tex, 0.0f, 1.0f);
  } else if (Constants.TextureFilter == FILTER_BILINEAR) {
    tex = Texture.Sample(LinearSampler, input.uv);
    tex = clamp(tex, 0.0f, 1.0f);
  } else {
    tex = float4(1.0f, 0.0f, 0.0f, 1.0f);
  }

  // CSC
  if (Constants.SrcColorSpace != Constants.DstColorSpace) {
    if (Constants.SrcColorSpace == COLOR_SPACE_LINEAR) {
      tex = float4(LinearToSRGB(tex.rgb), tex.w);
    } else {
      tex = float4(SRGBToLinear(tex.rgb), tex.w);
    }
  }

  if (Constants.DstPixelOffset.x != 0.0f ||
      Constants.DstPixelOffset.y != 0.0f) {
    col = col * float4(0, 0, 0, min(0.75f, tex.a)); 
  } else {
    col = col * tex;
  }
  return col;
};

