#include "commandqueue.h"

#include <cassert>
#include <system_error>

namespace chaos {

CommandQueue::CommandQueue(
    ComPtr<ID3D12Device> device, CommandListType type, int frame_count)
    : device_(device) {
  assert(frame_count > 0);
  contexts_.resize(frame_count);

  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  if (type == CommandListType::Direct) {
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  } else if (type == CommandListType::Copy) {
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
  } else if (type == CommandListType::Compute) {
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
  } else {
    assert(false && "not implemented.");
  }
  ThrowIfFailed(
      device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&cmdqueue_)));
  cmdqueue_->SetName(L"CommandQueue");

  for (int i = 0; i < frame_count; ++i) {
    ThrowIfFailed(device->CreateCommandAllocator(
        queue_desc.Type, IID_PPV_ARGS(&contexts_[i].cmdalloc)));

    wchar_t name[25]{};
    swprintf_s(name, L"CommandAllocator%u", i);
    contexts_[i].cmdalloc->SetName(name);
  }

  ThrowIfFailed(device->CreateCommandList(0, queue_desc.Type,
      contexts_[0].cmdalloc.Get(), nullptr, IID_PPV_ARGS(&cmdlist_)));
  ThrowIfFailed(cmdlist_->Close());
  cmdlist_->SetName(L"CommandList");

  ThrowIfFailed(device->CreateFence(
      contexts_[0].fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
  contexts_[0].fence_value++;
  fence_->SetName(L"Fence");

  fenceevent_.Attach(
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
  if (!fenceevent_.IsValid()) {
    throw std::system_error(std::error_code(static_cast<int>(GetLastError()),
                                std::system_category()),
        "CreateEventEx");
  }
}

CommandQueue::~CommandQueue() {}

void CommandQueue::Signal(int index) {
  const uint64_t value = contexts_[index].fence_value;
  ThrowIfFailed(cmdqueue_->Signal(fence_.Get(), value));
}

void CommandQueue::Wait(int index) {
  const uint64_t value = contexts_[index].fence_value;
  ThrowIfFailed(cmdqueue_->Signal(fence_.Get(), value));

  if (SUCCEEDED(fence_->SetEventOnCompletion(value, fenceevent_.Get()))) {
    WaitForSingleObjectEx(fenceevent_.Get(), INFINITE, FALSE);
    contexts_[index].fence_value++;
  }
}

void CommandQueue::WaitOnCompletion(int index, uint64_t value) {
  const uint64_t current = contexts_[index].fence_value;
  ThrowIfFailed(fence_->SetEventOnCompletion(current, fenceevent_.Get()));
  DWORD ret = WaitForSingleObjectEx(fenceevent_.Get(), INFINITE, FALSE);
  assert(ret == WAIT_OBJECT_0);
}

}  // namespace chaos
