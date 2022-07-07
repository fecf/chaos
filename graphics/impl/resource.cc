#include "resource.h"
#include "base/task.h"

namespace chaos {

ResourceDesc::ResourceDesc(ResourceUsage usage, size_t size) {
  this->usage = usage;
  this->size = size;
  this->pitch = size;
}

ResourceDesc::ResourceDesc(ResourceFormat format, int width, int height) {
  usage = ResourceUsage::Image;
  this->format = format;
  this->width = width;
  this->height = height;

  // constexpr int align = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  constexpr int align = 256;
  size_t pitch = width * format.bpp;
  const size_t alignedpitch = (pitch + align - 1u) & ~(align - 1u);
  const size_t alignedsize = alignedpitch * height;
  this->size = alignedsize;
  this->pitch = alignedpitch;
}

Resource::Resource(const ResourceDesc& resource_desc)
    : resource_desc_(resource_desc) {}

Resource::~Resource() {
  if (deleter_) {
    deleter_(*this);
  }
}

void* Resource::Map() {
  void* data = nullptr;
  CD3DX12_RANGE range{};
  ThrowIfFailed(resource_->Map(0, &range, &data));
  return data;
}

void Resource::Unmap() { resource_->Unmap(0, NULL); }

void Resource::Upload(void* ptr, size_t size) {
  void* dst = Map();
  ::memcpy(dst, ptr, size);
  Unmap();
}

void* Resource::MapStaging() {
  void* data = nullptr;
  CD3DX12_RANGE range{};
  ThrowIfFailed(staging_resource_->Map(0, &range, &data));
  return data;
}

void Resource::UnmapStaging() { staging_resource_->Unmap(0, NULL); }

ResourceGC::ResourceGC() {}

ResourceGC::~ResourceGC() { 
  Cleanup(UINT64_MAX); 
  chaos::task::dispatchQueue("gc")->wait();
}

void ResourceGC::Add(const Resource& resource, uint64_t fencevalue) {
  Garbage garbage;
  garbage.resource = resource.resource_;
  garbage.allocation = resource.allocation_;
  garbage.fencevalue = fencevalue;

  // std::string name = D3D12GetName(resource.resource_.Get());
  // DLOG_F("mark as garbage resource(%s) fencevalue(%llu).", name.c_str(), fencevalue);

  {
    std::lock_guard lock(mutex_);
    resources_.emplace_back(std::move(garbage));
  }
}

void ResourceGC::Cleanup(uint64_t fencevalue) {
  std::lock_guard lock(mutex_);
  size_t count = std::count_if(resources_.begin(), resources_.end(),
      [fencevalue](
          const Garbage& garbage) { return garbage.fencevalue < fencevalue; });
  if (count > 0) {
#if 1
    chaos::task::dispatchAsync("gc", [&](std::atomic<bool>&) {
      std::lock_guard lock(mutex_);
      size_t deleted =
          std::erase_if(resources_, [fencevalue](const Garbage& garbage) {
            return garbage.fencevalue < fencevalue;
          });
      if (deleted > 0) {
        // DLOG_F("erased %d resources fencevalue(%llu).", deleted, fencevalue);
      }
    });
#else
    size_t deleted =
        std::erase_if(resources_, [fencevalue](const Garbage& garbage) {
          return garbage.fencevalue < fencevalue;
        });
    if (deleted > 0) {
      DLOG_F("erased %d resources fencevalue(%llu).", deleted, fencevalue);
    }
#endif
  }
}

}  // namespace chaos