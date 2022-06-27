#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

#include "d3d12def.h"

namespace chaos {

class DescriptorHeap;

class DescriptorHandle {
 public:
  DescriptorHandle() : cpu(), gpu(), index(), heap() {}
  DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpu,
      D3D12_GPU_DESCRIPTOR_HANDLE gpu, int index, DescriptorHeap* heap)
      : cpu(cpu), gpu(gpu), index(index), heap(heap) {}

  D3D12_CPU_DESCRIPTOR_HANDLE cpu;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu;
  int index;
  DescriptorHeap* heap;
};

class CircularDescriptorHeap {
 public:
  CircularDescriptorHeap() = delete;
  CircularDescriptorHeap(ComPtr<ID3D12Device> device,
      D3D12_DESCRIPTOR_HEAP_TYPE type, int back_buffer_count);
  ~CircularDescriptorHeap();

  void Upload(int back_buffer_index, DescriptorHeap* src);
  DescriptorHeap* GetHeap(int back_buffer_index) const;
  int64_t GetLastUploaded(int back_buffer_index) const;
  DescriptorHandle GetDescriptorHandle(
      int back_buffer_index, int slot_index) const;

 private:
  ComPtr<ID3D12Device> device_;
  D3D12_DESCRIPTOR_HEAP_TYPE type_;
  int increment_size_;
  std::vector<std::unique_ptr<DescriptorHeap>> heaps_;
  std::vector<int64_t> last_uploaded_;
  mutable std::mutex mutex_;
};

class DescriptorHeap {
  friend class DescriptorHandleDeleter;

 public:
  DescriptorHeap() = delete;
  DescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type,
      int capacity, int preallocated, bool shader_visible = false);
  ~DescriptorHeap();

  ID3D12DescriptorHeap* GetHeap() const;
  DescriptorHandle Allocate();
  void Release(const DescriptorHandle& descriptor_handle);
  DescriptorHandle GetDescriptorHandle(int slot_index);
  int GetCapacity() const noexcept { return capacity_; }
  int GetIncrementSize() const noexcept { return increment_size_; }
  int64_t GetLastAllocated() const;

 private:
  void extendHeap(int new_capacity);

 private:
  ComPtr<ID3D12Device> device_;
  D3D12_DESCRIPTOR_HEAP_TYPE type_;
  int capacity_;
  int increment_size_;
  ComPtr<ID3D12DescriptorHeap> heap_;
  std::vector<int> slots_;
  std::vector<int> reserved_;
  uint64_t last_allocated_;
  bool shader_visible_;
  mutable std::mutex mutex_;
};

}  // namespace chaos
