#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <cassert>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstddef>
#include "src/hash_set_base.h"

template <typename T>
class HashSetSequential : public HashSetBase<T> {
 public:
  explicit HashSetSequential(size_t initial_capacity) {
    buckets_ = std::vector<std::vector<T>>(initial_capacity);
    size_ = 0;
  }

  bool Add(T element) override {
    size_t hash = std::hash<T>()(element);
    size_t index = hash % buckets_.size();
    auto &bucket = buckets_[index];
    for (const auto &elem: bucket) {
      if (elem == element) {
        return false;  // Element already exists
      }
    }
    bucket.push_back(element);
    size_++;
    if (Policy()) {
      Resize();
    }
    return true;
  }

  bool Remove(T element) override {
    size_t hash = std::hash<T>()(element);
    size_t index = hash % buckets_.size();
    auto &bucket = buckets_[index];
    for (size_t i = 0; i < bucket.size(); i++) {
      if (bucket[i] == element) {
        bucket.erase(bucket.begin() + i);
        size_ --;
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool Contains(T element) override {
    size_t hash = std::hash<T>()(element);
    size_t index = hash % buckets_.size();
    const auto &bucket = buckets_[index];

    for (const auto &elem: bucket) {
      if (elem == element) {
        return true;
      }
    }

    return false; 
  }

  [[nodiscard]] size_t Size() const override {
    return size_;
  }


private:
  size_t bucketThreshold_ = 4;
  size_t size_;
  std::vector<std::vector<T>> buckets_;
  
  [[nodiscard]] bool Policy() const {
    for (const auto &bucket : buckets_) {
      if (bucket.size() > bucketThreshold_) return true;
    }
    return false;
  }
  
  void Resize() {
    bucketThreshold_ = bucketThreshold_ * 2;
    int oldCapacity = buckets_.size();
    int newCapacity = oldCapacity * 2;
    std::vector<std::vector<T>> oldBuckets = buckets_;
    buckets_ = std::vector<std::vector<T>>(newCapacity);
    for (size_t i = 0; i < newCapacity; i++) {
      buckets_[i] = std::vector<T>();
    }
    for (const auto &bucket : oldBuckets) {
      for (const auto &element : bucket) {
        size_t hash = std::hash<T>()(element);
        size_t index = hash % newCapacity;
        buckets_[index].push_back(element);
      }
    }
  }
};

#endif  // HASH_SET_SEQUENTIAL_H