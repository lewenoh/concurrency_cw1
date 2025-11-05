#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include <cassert>

#include "src/hash_set_base.h"

struct AtomicMarkableReference {
    std::pair<std::thread::id, bool> data;
    std::mutex m;

    AtomicMarkableReference() : data{std::thread::id(), false} {}

    void store(std::thread::id tid, bool owner) {
        std::lock_guard<std::mutex> lock(m);
        data = {tid, owner};
    }

    std::pair<std::thread::id, bool> load() {
        std::lock_guard<std::mutex> lock(m);
        return data;
    }
};

template <typename T>
class HashSetRefinable : public HashSetBase<T> {
 public:
  explicit HashSetRefinable(size_t initical_capacity) {
    buckets_ = std::vector<std::vector<T>>(initial_capacity);
    size_.store(0, std::memory_order_relaxed);
    
    locks_ = std::vector<std::mutex>(initial_capacity);
  }

  bool Add(T element) final {
    size_t hash = std::hash<T>()(element);
    bool need_resize = false;

    { 
      bool mark = true;
      std::thread::id me = std::this_thread::get_id();
      std::thread::id who;
      
      
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

    need_re
    size = (bucket.size() > bucketThreshold_);

    }
    if (need_resize) {
      std::lock_guard<std::mutex> resize_guard(resize_mutex_); // global resizer mutex
      if (Policy()) Resize(); // Policy() will acquire all stripe locks safely
    }

    return true;
  }


  bool Remove(T /*elem*/) final {
    assert(false && "Not implemented yet");
    return false;
  }

  [[nodiscard]] bool Contains(T /*elem*/) final {
    assert(false && "Not implemented yet");
    return false;
  }

  [[nodiscard]] size_t Size() const final {
    assert(false && "Not implemented yet");
    return 0u;
  }

  private:
    AtomicMarkableReference owner; 
};

#endif  // HASH_SET_REFINABLE_H
