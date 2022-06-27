#pragma once

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace chaos {

// TODO:
template <typename T>
class HistoryList {
 public:
  bool can_back() const { return !back_.empty(); }
  bool can_forward() const { return !forward_.empty(); }
  const std::deque<T>& list_back() const { return back_; }
  const std::deque<T>& list_forward() const { return forward_; }
  T back() { return {}; }
  T forward() { return {}; }

 private:
  std::deque<T> back_;
  std::deque<T> forward_;
};

template <typename key_t, typename value_t>
class LRUCache {
 public:
  using entry_t = std::pair<key_t, value_t>;
  using iter_t = std::list<entry_t>::iterator;
  using const_iter_t = std::list<entry_t>::const_iterator;
  using reverse_iter_t = std::list<entry_t>::reverse_iterator;
  using const_reverse_iter_t = std::list<entry_t>::const_reverse_iterator;

  LRUCache() : capacity_(SIZE_MAX) {}
  LRUCache(size_t capacity) : capacity_(capacity) {}
  ~LRUCache() {}

  LRUCache(const LRUCache&) { __debugbreak();
  }
  LRUCache& operator=(const LRUCache&) { 
    __debugbreak();
    return *this;
  }
  LRUCache(LRUCache&&) = default;
  LRUCache& operator=(LRUCache&&) = default;

  bool empty() const { return list_.empty(); }

  entry_t top() const { return (*list_.cbegin()); }
  
  iter_t begin() { return list_.begin(); }
  const_iter_t begin() const { return list_.begin(); }
  iter_t end() { return list_.end(); }
  const_iter_t end() const { return list_.end(); }

  reverse_iter_t rbegin() { return list_.rbegin(); }
  const_reverse_iter_t crbegin() const { return list_.rbegin(); }
  reverse_iter_t rend() { return list_.rend(); }
  const_reverse_iter_t crend() const { return list_.rend(); }

  void put(const key_t& key, const value_t& value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      list_.erase(it->second);
      map_.erase(key);
    }

    list_.push_front(std::make_pair(key, value));
    map_.insert({key, list_.begin()});
    clean();
  }

  bool get(const key_t& key, value_t& value) {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return false;
    }
    list_.splice(list_.begin(), list_, it->second);
    value = it->second->value;
    return true;
  }

  size_t capacity() const noexcept { return capacity_; }

  void setCapacity(size_t capacity) { 
    capacity_ = capacity; 
    clean();
  }

  void enumerate(std::function<void(const key_t&, const value_t&)> callback) {
    for (auto entry : list_) {
      callback(entry.key, entry.value);
    }
  }

 private:
  void clean() {
    while (map_.size() > capacity_) {
      map_.erase(list_.back().first);
      list_.pop_back();
    }
  }

 private:
  std::list<entry_t> list_;
  std::unordered_map<key_t, decltype(list_.begin())> map_;
  size_t capacity_;
};

}  // namespace chaos
