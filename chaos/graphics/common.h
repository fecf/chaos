#pragma once

#include "extras/linalg.h"

struct Vertex {
  linalg::aliases::float3 pos;
  linalg::aliases::float3 normal;
  linalg::aliases::float2 uv;
};

