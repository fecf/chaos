#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#ifdef _MSC_VER
#include "extras/linalg.h"
#endif

#define ROOT_DESCRIPTOR_ROOT_CONSTANTS_SLOT 0
#define ROOT_DESCRIPTOR_VS_CBV_SLOT 1
#define ROOT_DESCRIPTOR_PS_CBV_SLOT 2
#define ROOT_DESCRIPTOR_PS_SRV_SLOT 3
#define ROOT_DESCRIPTOR_PS_SAMPLER_SLOT 4
#define ROOT_DESCRIPTOR_PARAMS 5

#define COLOR_SPACE_SRGB 0
#define COLOR_SPACE_LINEAR 1

#define FILTER_NEAREST 0
#define FILTER_BILINEAR 1

struct VsCBVDesc {};
struct PsCBVDesc {};

#ifdef _MSC_VER
#define column_major
#endif

// root 32-bit constants
struct RootConst {
  column_major linalg::aliases::float4x4 ProjectionMatrix;  // vs:
  int SrcColorSpace;
  int DstColorSpace;
  linalg::aliases::float2 DstPixelOffset;
  int TextureFilter;
};

// constant buffer per frame
struct VertexCB {};
struct PixelCB {};

#endif
