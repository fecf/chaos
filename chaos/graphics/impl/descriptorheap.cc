#include "descriptorheap.h"

#include <stdexcept>

namespace chaos {

DescriptorHeap::DescriptorHeap(ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, int capacity, int preallocated,
    bool shader_visible)
    : device_(device),
      type_(type),
      capacity_(capacity),
      increment_size_(device->GetDescriptorHandleIncrementSize(type)),
      shader_visible_(shader_visible),
      last_allocated_(0) {
  assert(capacity > 0);

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = capacity;
  desc.Type = type_;
  if (shader_visible) {
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  }

  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
  if (FAILED(hr)) {
    throw std::runtime_error("failed CreateDescriptorHeap().");
  }

  slots_.resize(capacity);
  std::iota(slots_.begin(), slots_.end(), 0);  // 0 ... size - 1

  for (int i = 0; i < preallocated; ++i) {
    Allocate();
  }
}

DescriptorHeap::~DescriptorHeap() {}

ID3D12DescriptorHeap* DescriptorHeap::GetHeap() const { 
  std::lock_guard lock(mutex_);
  return heap_.Get(); 
}

DescriptorHandle DescriptorHeap::Allocate() {
  std::lock_guard lock(mutex_);

  LARGE_INTEGER lpc{};
  ::QueryPerformanceCounter(&lpc);
  last_allocated_ = lpc.QuadPart;

  if (slots_.empty()) {
    extendHeap(capacity_ * 2);
  }

  int index = slots_.front();
  slots_.erase(slots_.begin());
  reserved_.push_back(index);

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
  cpu.ptr += (index * increment_size_);
  gpu.ptr += (index * increment_size_);
  DescriptorHandle descriptor_handle(cpu, gpu, index, this);
  return descriptor_handle;
}

void DescriptorHeap::Release(const DescriptorHandle& desc) {
  std::lock_guard lock(mutex_);

  LARGE_INTEGER lpc{};
  ::QueryPerformanceCounter(&lpc);
  last_allocated_ = lpc.QuadPart;

  slots_.push_back(desc.index);
  auto it = std::find(reserved_.begin(), reserved_.end(), desc.index);
  assert(it != reserved_.end());
  reserved_.erase(it);
}

DescriptorHandle DescriptorHeap::GetDescriptorHandle(int slot_index) {
  std::lock_guard lock(mutex_);

  for (int i = 0; i < (int)reserved_.size(); ++i) {
    if (reserved_[i] == slot_index) {
      D3D12_CPU_DESCRIPTOR_HANDLE cpu =
          heap_->GetCPUDescriptorHandleForHeapStart();
      cpu.ptr += slot_index * increment_size_;
      if (shader_visible_) {
        D3D12_GPU_DESCRIPTOR_HANDLE gpu =
            heap_->GetGPUDescriptorHandleForHeapStart();
        gpu.ptr += slot_index * increment_size_;
        DescriptorHandle descriptor_handle(cpu, gpu, slot_index, this);
        return descriptor_handle;
      } else {
        DescriptorHandle descriptor_handle(
            cpu, {} , slot_index, this);
        return descriptor_handle;
      }
    }
  }

  throw std::domain_error("slot not found.");
}

void DescriptorHeap::extendHeap(int new_capacity) {
  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = new_capacity;
  desc.Type = type_;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  ComPtr<ID3D12DescriptorHeap> new_heap;
  HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&new_heap));
  if (FAILED(hr)) {
    throw std::runtime_error("failed CreateDescriptorHeap().");
  }

  int add_size = new_capacity - capacity_;
  int old_size = (int)slots_.size();
  slots_.resize(old_size + add_size);
  std::iota(slots_.begin() + old_size, slots_.end(), capacity_);

  auto dst = new_heap->GetCPUDescriptorHandleForHeapStart();
  auto src = heap_->GetCPUDescriptorHandleForHeapStart();
  device_->CopyDescriptorsSimple(capacity_, dst, src, type_);

  heap_ = new_heap;
  capacity_ = new_capacity;
}

int64_t DescriptorHeap::GetLastAllocated() const { return last_allocated_; }

CircularDescriptorHeap::CircularDescriptorHeap(ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, int back_buffer_count)
    : device_(device),
      type_(type),
      last_uploaded_(),
      increment_size_(device->GetDescriptorHandleIncrementSize(type)) {
  std::lock_guard lock(mutex_);

  for (int i = 0; i < back_buffer_count; ++i) {
    heaps_.emplace_back(new DescriptorHeap(device, type, 16, 0, true));
    last_uploaded_.emplace_back(0);
  }
}

CircularDescriptorHeap::~CircularDescriptorHeap() {}

void CircularDescriptorHeap::Upload(
    int back_buffer_index, DescriptorHeap* src) {
  std::lock_guard lock(mutex_);

  LARGE_INTEGER lpc{};
  ::QueryPerformanceCounter(&lpc);
  last_uploaded_[back_buffer_index] = lpc.QuadPart;

  const int gpu_capacity = heaps_[back_buffer_index]->GetCapacity();
  const int cpu_capacity = src->GetCapacity();
  if (gpu_capacity < cpu_capacity) {
    heaps_[back_buffer_index] =
        std::unique_ptr<DescriptorHeap>(new DescriptorHeap(
            device_, type_, cpu_capacity, cpu_capacity, true));
  }
  ID3D12DescriptorHeap* src_heap = src->GetHeap();
  ID3D12DescriptorHeap* dst_heap = heaps_[back_buffer_index]->GetHeap();
  auto src_heap_cpu = src_heap->GetCPUDescriptorHandleForHeapStart();
  auto dst_heap_cpu = dst_heap->GetCPUDescriptorHandleForHeapStart();
  device_->CopyDescriptorsSimple(
      cpu_capacity, dst_heap_cpu, src_heap_cpu, type_);
}

DescriptorHeap* CircularDescriptorHeap::GetHeap(int back_buffer_index) const {
  std::lock_guard lock(mutex_);

  return heaps_[back_buffer_index].get();
}

int64_t CircularDescriptorHeap::GetLastUploaded(int back_buffer_index) const {
  std::lock_guard lock(mutex_);

  return last_uploaded_[back_buffer_index];
}

DescriptorHandle CircularDescriptorHeap::GetDescriptorHandle(
    int back_buffer_index, int slot_index) const {
  std::lock_guard lock(mutex_);

  auto heap = heaps_[back_buffer_index].get();

  D3D12_CPU_DESCRIPTOR_HANDLE cpu =
      heap->GetHeap()->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE gpu =
      heap->GetHeap()->GetGPUDescriptorHandleForHeapStart();
  cpu.ptr += slot_index * increment_size_;
  gpu.ptr += slot_index * increment_size_;
  DescriptorHandle descriptor_handle(cpu, gpu, slot_index, heap);
  return descriptor_handle;
}

}  // namespace chaos
