#ifndef HASH_SET_STRIPED_H
#define HASH_SET_STRIPED_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetStriped : public HashSetBase<T> {
public:
  explicit HashSetStriped(size_t initial_capacity)
      : original_capacity_(initial_capacity), capacity_(initial_capacity),
        size_(0), mutexes_(std::vector<std::mutex>(initial_capacity)),
        table_(
            std::vector<std::vector<T>>(initial_capacity, std::vector<T>())) {}

  bool Add(T elem) final {
    size_t hash = std::hash<T>()(elem) % capacity_;
    std::scoped_lock<std::mutex> lock(
        mutexes_[std::hash<T>()(elem) % original_capacity_]);
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
      size_++;
      table_[hash].push_back(elem);
      if (size_ > 4 * capacity_) {
        std::cout << "Resizing: ";
        capacity_ *= 2;
        std::vector<std::vector<T>> old_table = table_;
        table_ = std::vector<std::vector<T>>(capacity_, std::vector<T>());
        for (auto &bucket : old_table) {
          for (T curr_elem : bucket) {
            size_t curr_hash = std::hash<T>()(curr_elem) % capacity_;
            table_[curr_hash].push_back(curr_elem);
          }
        }
        std::cout << "Resizing done" << std::endl;
      }
      return true;
    } else {
      return false;
    }
  }

  bool Remove(T elem) final {
    size_t hash = std::hash<T>()(elem) % capacity_;
    std::scoped_lock<std::mutex> lock(
        mutexes_[std::hash<T>()(elem) % original_capacity_]);
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
    size_t hash = std::hash<T>()(elem) % capacity_;
    std::scoped_lock<std::mutex> lock(
        mutexes_[std::hash<T>()(elem) % original_capacity_]);
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    return it != table_[hash].end();
  }

  [[nodiscard]] size_t Size() const final { return size_.load(); }

private:
  const size_t original_capacity_;
  size_t capacity_;
  std::atomic<size_t> size_;
  std::vector<std::mutex> mutexes_;
  std::vector<std::vector<T>> table_;
};

#endif // HASH_SET_STRIPED_H
