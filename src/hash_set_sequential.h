#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <algorithm>
#include <cassert>
#include <functional>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetSequential : public HashSetBase<T> {
public:
  explicit HashSetSequential(size_t initial_capacity)
      : capacity_(initial_capacity), table_(std::vector<std::vector<T>>(
                                         initial_capacity, std::vector<T>())) {}

  bool Add(T elem) final {
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
        size_++;
        table_[hash].push_back(elem);
        if (size_ > 4*capacity_) {
            Resize();
        }
        return true;
    } else {
        return false;
    }
  }

  bool Remove(T elem) final {
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
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    return it != table_[hash].end();
  }

  [[nodiscard]] size_t Size() const final { return size_; }
  
    void Resize() {
        capacity_ *= 2;
        std::vector<std::vector<T>> old_table = table_;
        table_ = std::vector<std::vector<T>>(capacity_, std::vector<T>());
        for (auto& bucket : old_table) {
            for (T elem : bucket) {
                size_t hash = std::hash<T>()(elem) % capacity_;
                table_[hash].push_back(elem);
            }
        }
    }

private:
  size_t capacity_;
  size_t size_ = 0;
  std::vector<std::vector<T>> table_;
};

#endif // HASH_SET_SEQUENTIAL_H
