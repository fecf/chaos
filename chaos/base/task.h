#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace chaos {

struct Timer {
  Timer();
  Timer(const std::string& name);
  virtual ~Timer();

  double elapsed() const;

 private:
  std::string name_;
  std::chrono::high_resolution_clock::time_point start_;
};

namespace task {

using func_t = std::function<void(std::atomic<bool>&)>;

struct Task {
  int priority;
  func_t func;
};

class DispatchQueue {
 public:
  DispatchQueue();
  ~DispatchQueue();

  void cancel();
  void wait();
  void enqueue(Task&& task);
  void setThreadCount(int size);
  int waitingTaskCount() const;

 private:
  struct Worker {
    Worker();
    ~Worker();
    std::jthread thread;
    std::atomic<bool> cancel;
  };
  void workerThread(Worker* w);

  std::vector<Worker> workers_;

  struct Comparer {
    bool operator()(const Task& a, const Task& b) {
      return a.priority > b.priority;
    }
  };
  std::priority_queue<Task, std::vector<Task>, Comparer> pq_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> exit_;
  std::atomic<int> running_threads_;
};

DispatchQueue* dispatchQueue();
DispatchQueue* dispatchQueue(const char* id);

void dispatchAsync(const char* id, func_t func, int priority = 0);
void dispatchAsync(func_t func);
void enumerateDispatchQueues(std::function<void(const char*)> callback);

}  // namespace task

}  // namespace chaos
