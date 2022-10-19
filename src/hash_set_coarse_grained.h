#ifndef HASH_SET_COARSE_GRAINED_H
#define HASH_SET_COARSE_GRAINED_H

#include <algorithm>
#include <cassert>
#include <functional>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetCoarseGrained : public HashSetBase<T> {
public:
  explicit HashSetCoarseGrained(size_t initial_capacity)
      : capacity_(initial_capacity), table_(std::vector<std::vector<T>>(
                                         initial_capacity, std::vector<T>())) {}

  bool Add(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
      size_++;
      table_[hash].push_back(elem);
      return true;
    } else {
      return false;
    }
  }

  bool Remove(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
      return false;
    } else {
      size_--;
      table_[hash].erase(it);
      return true;
    }
  }

  [[nodiscard]] bool Contains(T elem) final {
    std::scoped_lock<std::mutex> lock(mutex_);
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    return it != table_[hash].end();
  }

  [[nodiscard]] size_t Size() const final { return size_; }

private:
  const size_t capacity_;
  size_t size_ = 0;
  std::mutex mutex_;
  std::vector<std::vector<T>> table_;
};

#endif // HASH_SET_COARSE_GRAINED_H
