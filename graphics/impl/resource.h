#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "d3d12def.h"
#include "descriptorheap.h"
#include "../../base/types.h"

namespace chaos {

struct ResourceFormat {
  static ResourceFormat RGBA8() {
    return ResourceFormat{DXGI_FORMAT_R8G8B8A8_UNORM, 4};
  }
  static ResourceFormat RGBA16() {
    return ResourceFormat{DXGI_FORMAT_R16G16B16A16_UNORM, 8};
  }
  static ResourceFormat RGBA16F() {
    return ResourceFormat{DXGI_FORMAT_R16G16B16A16_FLOAT, 8};
  }
  static ResourceFormat RGBA32F() {
    return ResourceFormat{DXGI_FORMAT_R32G32B32A32_FLOAT, 16};
  }
  static ResourceFormat BGRA8() {
    return ResourceFormat{DXGI_FORMAT_B8G8R8A8_UNORM, 4};
  }

  static ResourceFormat FromPixelFormat(PixelFormat format) {
    if (format == PixelFormat::BGRA8) {
      return ResourceFormat::BGRA8();
    }
    else if (format == PixelFormat::RGBA16) {
      return ResourceFormat::RGBA16();
    }
    else if (format == PixelFormat::RGBA32F) {
      return ResourceFormat::RGBA32F();
    }
    else if (format == PixelFormat::RGBA8) {
      return ResourceFormat::RGBA8();
    }
    else {
      throw std::domain_error("unknown format.");
    }
  }

  operator DXGI_FORMAT() const { return dxgi_format; }

  DXGI_FORMAT dxgi_format;
  int bpp;
};

enum class ResourceUsage {
  Constant = 4,
  Unordered = 8,
  Vertex = 16,
  Index = 32,
  Normal = 64,
  UV = 128,
  Image = 256,
  Staging = 512,
};

class ResourceDesc {
 public:
  ResourceDesc() = delete;
  ResourceDesc(ResourceUsage usage, size_t size);
  ResourceDesc(ResourceFormat format, int width, int height);
  ~ResourceDesc() = default;
  ResourceDesc(const ResourceDesc&) = default;
  ResourceDesc& operator=(const ResourceDesc&) = default;

  ResourceUsage usage;
  size_t size;
  size_t pitch;

  ResourceFormat format;
  int width;
  int height;
};

class Resource {
  friend class Context;
  friend class ResourceGC;

 private:
  Resource(const ResourceDesc& resource_desc);

 public:
  ~Resource();

  const ResourceDesc& GetDesc() const noexcept { return resource_desc_; }
  ID3D12Resource2* GetResource() const { return resource_.Get(); }
  ID3D12Resource2* GetStagingResource() const {
    return staging_resource_.Get();
  }
  const DescriptorHandle& GetDescriptorHandle() const {
    return descriptor_handle_;
  };

  void* Map();
  void Unmap();
  void Upload(void* ptr, size_t size);

  void* MapStaging();
  void UnmapStaging();

 private:
  ComPtr<ID3D12Resource2> resource_;
  ComPtr<ID3D12Resource2> staging_resource_;
  ComPtr<D3D12MA::Allocation> allocation_;
  ComPtr<D3D12MA::Allocation> staging_allocation_;
  DescriptorHandle descriptor_handle_;
  ResourceDesc resource_desc_;
  std::function<void(const Resource&)> deleter_;
};

class ResourceGC {
 public:
  ResourceGC();
  ~ResourceGC();

  // Extend lifetime of d3d12 resource
  void Add(const Resource& resource, uint64_t fencevalue);

  // Release d3d12 resources which have passed recorded fence value
  void Cleanup(uint64_t fencevalue);

 private:
  std::mutex mutex_;
  struct Garbage {
    ComPtr<ID3D12Resource> resource;
    ComPtr<D3D12MA::Allocation> allocation;
    uint64_t fencevalue;
  };
  std::vector<Garbage> resources_;
};

}  // namespace chaos
