#ifndef HASH_SET_STRIPED_H
#define HASH_SET_STRIPED_H

#include <cassert>
#include <vector>
#include <mutex>
#include <cstddef>
#include <atomic>
#include <functional>
#include <algorithm>
#include <thread>

#include "src/hash_set_base.h"

template <typename T>
class HashSetStriped : public HashSetBase<T> {
 public:
  explicit HashSetStriped(size_t initial_capacity) {
    buckets_ = std::vector<std::vector<T>>(initial_capacity);
    size_.store(0, std::memory_order_relaxed);
    
    locks_ = std::vector<std::mutex>(initial_capacity);
    stripeCount_ = initial_capacity; // table will resize but not locks 
  }
  
  bool Add(T element) final {
    size_t hash = std::hash<T>()(element);

    bool need_resize = false;

    // create new scope so the stripe lock is released when it finishes,
    // preventing holding a stripe lock while holding other locks 
    // which would cause a deadlock
    { 
      std::lock_guard<std::mutex> lock(locks_[hash % stripeCount_]);
      size_t index = hash % buckets_.size();
      auto &bucket = buckets_[index];
      for (const auto &elem: bucket) {
      if (elem == element) {
        return false;  // Element already exists
      }
    }
    bucket.push_back(element);
    size_.fetch_add(1, std::memory_order_relaxed);

    // cheap local check first before acquiring all locks in Resize
    need_resize = (bucket.size() > bucketThreshold_);

    }

    if (need_resize) {

      std::lock_guard<std::mutex> resize_guard(resize_mutex_); // global resizer mutex

      if (Policy()) Resize(); // Policy() will acquire all stripe locks safely
    }

    return true;
  }

  bool Remove(T element) final {
    size_t hash = std::hash<T>()(element);

    std::lock_guard<std::mutex> lock(locks_[hash % stripeCount_]);
    size_t index = hash % buckets_.size();
    auto &bucket = buckets_[index];
    for (size_t i = 0; i < bucket.size(); i++) {
      if (bucket[i] == element) {
        bucket.erase(bucket.begin() + i);
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool Contains(T element) final {
    size_t hash = std::hash<T>()(element);

    std::lock_guard<std::mutex> lock(locks_[hash % stripeCount_]);
    size_t index = hash % buckets_.size();
    auto &bucket = buckets_[index];
    for (const auto &elem: bucket) {
      if (elem == element) {
        return true;
      }
    }

    return false; 
  }

  [[nodiscard]] size_t Size() const final {
    return size_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<size_t> size_;  
  std::vector<std::vector<T>> buckets_;
  std::vector<std::mutex> locks_;
  size_t bucketThreshold_ = 4;
  size_t stripeCount_;
  std::mutex resize_mutex_;
  
  [[nodiscard]] bool Policy() {
    std::vector<std::unique_lock<std::mutex>> guards;
    guards.reserve(stripeCount_);
    for (size_t i = 0; i < stripeCount_; ++i) guards.emplace_back(locks_[i]);

    for (const auto &bucket : buckets_) {
      if (bucket.size() > bucketThreshold_) {
        return true;
      };
    }

    return false;
  }

  void Resize() {
    std::vector<std::unique_lock<std::mutex>> guards;
    guards.reserve(stripeCount_);
    for (size_t i = 0; i < stripeCount_; ++i) guards.emplace_back(locks_[i]);
    bucketThreshold_ = bucketThreshold_ * 2;
    int oldCapacity = buckets_.size();
    int newCapacity = oldCapacity * 2;
    std::vector<std::vector<T>> oldBuckets = buckets_;
    buckets_ = std::vector<std::vector<T>>(newCapacity);
  
    for (const auto &bucket : oldBuckets) {
      for (const auto &element : bucket) {
        size_t hash = std::hash<T>()(element);
        size_t index = hash % newCapacity;
        buckets_[index].push_back(element);
      }
    }

  }
};

#endif  // HASH_SET_STRIPED_H
