#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include <cassert>

#include "src/hash_set_base.h"

template <typename T>
class HashSetRefinable : public HashSetBase<T> {
 public:
  explicit HashSetRefinable(size_t initical_capacity) {
    auto buckets_ = std::make_shared<vector<std::vector<T>>>(initial_capacity);
    std::atomic_store(&buckets_ptr_, buckets_);
    
    size_.store(0, std::memory_order_relaxed);
    
    auto locks_ = std::make_shared<std::vector<std::mutex>>(initial_capacity);
    std::atomic_store(&locks_ptr_, locks_);
  }

  bool Add(T element) final {
    size_t hash = std::hash<T>()(element);
    
    while (true) {
      auto buckets_ = std::atomic_load(&buckets_ptr_);
      auto locks_ = std::atomic_load(&locks_ptr_);

      size_t bidx = hash % buckets->size();
      size_t lidx = hash % locks->size();

      std::unique_lock<std::mutex> lock((*locks_)[lidx]);

      if (buckets_ != std::atomic_load(&buckets_ptr_) ||
          locks_ != std::atomic_load(&locks_ptr_)) {
        continue; //table has been resized - retry
      }

      auto &bucket = (*buckets_)[bidx];
      for (const auto &elem: bucket) {
        if (elem == element) {
          return false; 
        }
      }

      bucket.push_back(element);
      size_.fetch_add(1, std::memory_order_relaxed);
      
      bool need_resize = (bucket.size() > bucketsThreshold_);
      lock.unlock();

      if (need_resize) TryResize(buckets_, locks_);
      return true;
    }

  }


  bool Remove(T element) final {
    size_t hash = std::hash<T>()(element);

    while (true) {
      auto buckets_ = std::atomic_load(&buckets_ptr_);
      auto locks_ = std::atomic_load(&locks_ptr_);

      size_t bidx = hash % buckets_->size();
      size_t lidx = hash % locks_->size();

      std::unique_lock<std::mutex> lock((*locks_)[lidx]);
      
      if (buckets_ != std::atomic_load(&buckets_ptr_) ||
          locks_ != std::atomic_load(&locks_ptr_)) {
        continue; //table has been resized - retry
      }

      auto &bucket = (*buckets_)[bidx];
      
      // rehash elements
      for (auto it = bucket.begin(); it != bucket.end(); ++it) {
        if (*it == element) {
          bucket.erase(it);
          size_.fetch_sub(1, std::memory_order_relaxed);
          return true;
        }
      }
      return false;
    }
  }


  [[nodiscard]] bool Contains(T element) final {
    size_t hash = std::hash<T>()(element);

    while (true) {
      auto buckets_ = std::atomic_load(&buckets_ptr_);
      auto locks_ = std::atomic_load(&locks_ptr_);

      size_t bidx = hash % buckets_->size();
      size_t lidx = hash % locks_->size();

      std::unique_lock<std::mutex> lock((*locks_)[lidx]);
      
      if (buckets_ != std::atomic_load(&buckets_ptr_) ||
          locks+ != std::atomic_load(&locks_ptr_)) {
        continue; //table has been resized - retry
      }

      auto &bucket = (*buckets_)[bidx];
      for (const auto &e : bucket) {
        if (e == element) return true;
      }
      return false;
    }
  }

  [[nodiscard]] size_t Size() const final {
    return size_.load(std::memory_order_relaxed);
  }

 private: 
  std::atomic<size_t> size_{0};  
  std::shared_ptr<std::vector<std::vector<T>>> buckets_ptr_;
  std::shared_ptr<std::vector<std::mutex>> locks_ptr_;
  size_t bucketThreshold_ = 4;

  std::mutex resize_mutex_;
  

  void TryResize(std::shared_ptr<std::vector<std::vector<T>>> old_buckets,
                 std::shared_ptr<std::vector<std::mutex>> old_locks) {
    
    std::unique_lock<std::mutex> resize_lock(resize_mutex_, std::try_to_lock);
    if (!resize_lock.owns_lock()) {
      return; // another thread is resizing
    }
    // quick check: ensure caller's snapshot is still current
    if (std::atomic_load(&buckets_ptr_) != old_buckets ||
        std::atomic_load(&locks_ptr_) != old_locks) {
      return;
    }

    // acquire all stripe locks
    std::vector<std::unique_lock<std::mutex>> stripe_locks;
    stripe_locks.reserve(old_locks->size());
    for (size_t i = 0; i < old_locks->size(); ++i) stripe_locks.emplace_back((*old_locks)[i]);
    
    // re-check after acquiring stripe locks
    if (std::atomic_load(&buckets_ptr_) != old_buckets ||
        std::atomic_load(&locks_ptr_) != old_locks) {
      return;
    }

    // check if resize is needed
    for (const auto &bucket : *old_buckets) {
      if (bucket.size() <= bucketThreshold_) {
        return; // no need to resize
      };
    }

    bucketThreshold_ = bucketThreshold_ * 2;
    size_t newCapacity = old_buckets->size() * 2;
    auto new_buckets = std::make_shared<std::vector<std::vector<T>>>(newCapacity);
  
    for (const auto &bucket : *old_buckets) {
      for (const auto &element : bucket) {
        size_t hash = std::hash<T>()(element);
        size_t index = hash % newCapacity;
        (*new_buckets)[index].push_back(element);
      }
    }

    auto new_locks = std::make_shared<std::vector<std::mutex>>(newCapacity);

    std::atomic_store(&buckets_ptr_, new_buckets);
    std::atomic_store(&locks_ptr_, new_locks);

  }
};

#endif  // HASH_SET_REFINABLE_H
