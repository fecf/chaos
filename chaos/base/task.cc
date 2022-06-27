#include "task.h"

#include "minlog.h"

namespace chaos {

Timer::Timer() : name_(), start_(std::chrono::high_resolution_clock::now()) {}

Timer::Timer(const std::string& name)
    : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

Timer::~Timer() {}

double Timer::elapsed() const {
  std::chrono::time_point end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start_;
  return elapsed.count();
}

namespace task {

std::mutex g_dispatch_queue_mutex;
std::unordered_map<std::string, DispatchQueue> g_dispatch_queue_map;

constexpr const char* kDefaultDispatchQueueId = "global";

DispatchQueue::Worker::Worker() : cancel(false) {}

DispatchQueue::Worker::~Worker() {
  if (thread.joinable()) {
    thread.join();
  }
}

DispatchQueue::DispatchQueue() : exit_(false) {
  setThreadCount(1);
}

DispatchQueue::~DispatchQueue() {
  exit_ = true;
  cv_.notify_all();
  for (Worker& w : workers_) {
    if (w.thread.joinable()) {
      w.thread.join();
    }
  }
}

void DispatchQueue::cancel() {
  std::unique_lock lock(mutex_);
  pq_ = {};
  for (Worker& w : workers_) {
    w.cancel = true;
  }
}

void DispatchQueue::wait() {
  std::unique_lock lock(mutex_);
  cv_.wait(
      lock, [&] { return (exit_ || pq_.empty()) && (running_threads_ <= 0); });
}

void DispatchQueue::enqueue(Task&& task) {
  {
    std::lock_guard lock(mutex_);
    pq_.push(std::move(task));
  }
  cv_.notify_one();
}

void DispatchQueue::setThreadCount(int size) {
  wait();

  {
    std::lock_guard lock(mutex_);
    exit_ = true;
  }
  cv_.notify_all();
  workers_.clear();

  {
    std::lock_guard lock(mutex_);
    exit_ = false;
    workers_ = std::vector<Worker>(size);
    for (Worker& w : workers_) {
      if (!w.thread.native_handle()) {
        w.thread = std::jthread(&DispatchQueue::workerThread, this, &w);
      }
    }
  }
}

int DispatchQueue::queuedTaskCount() const { return (int)pq_.size(); }

void DispatchQueue::workerThread(Worker* w) {
  task::Task task;
  while (!exit_) {
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [&] { return exit_ || !pq_.empty(); });
      if (exit_) {
        break;
      }
      task = pq_.top();
      pq_.pop();
    }
    cv_.notify_all();

    running_threads_++;
    if (!w->cancel) {
      task.func(w->cancel);
    }
    {
      std::unique_lock lock(mutex_);
      w->cancel = false;
      running_threads_--;
    }
    cv_.notify_all();
  }
}

DispatchQueue* dispatchQueue() {
  return &g_dispatch_queue_map[kDefaultDispatchQueueId];
}

DispatchQueue* dispatchQueue(const char* id) {
  return &g_dispatch_queue_map[id];
}

void dispatchAsync(const char* id, func_t func, int priority) {
  dispatchQueue(id)->enqueue({priority, func});
}

void dispatchAsync(func_t func) {
  dispatchAsync(kDefaultDispatchQueueId, func, 0);
}

void enumerateDispatchQueues(std::function<void(const char*)> callback) {
  std::lock_guard lock(g_dispatch_queue_mutex);
  for (const auto& kv : g_dispatch_queue_map) {
    callback(kv.first.c_str());
  }
}

}  // namespace task

}  // namespace chaos
