#pragma once

#include "d3d12def.h"

#include <vector>

namespace chaos {

class CommandQueue {
 public:
  enum class CommandListType { Direct, Compute, Copy };

  CommandQueue(ComPtr<ID3D12Device> device, CommandListType type, int frame_count);
  ~CommandQueue();

  ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return cmdlist_; }
  ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return cmdqueue_; }
  ComPtr<ID3D12CommandAllocator> GetCommandAllocator(int index) const {
    return contexts_[index].cmdalloc;
  }
  ComPtr<ID3D12Fence> GetFence() const { return fence_; }
  uint64_t GetFenceValue(int index) const { return contexts_[index].fence_value; }
  void SetFenceValue(int index, uint64_t value) { contexts_[index].fence_value = value; }

  void Signal(int index);
  void Wait(int index);
  void WaitOnCompletion(int index, uint64_t value);

 private:
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12GraphicsCommandList> cmdlist_;
  ComPtr<ID3D12CommandQueue> cmdqueue_;
  ComPtr<ID3D12Fence> fence_;
  Wrappers::Event fenceevent_;

  struct FrameContext {
    ComPtr<ID3D12CommandAllocator> cmdalloc;
    uint64_t fence_value;
  };
  std::vector<FrameContext> contexts_;
};

}  // namespace chaos
