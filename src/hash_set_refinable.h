#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include "src/hash_set_base.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <functional>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <condition_variable>

template <typename T> class HashSetRefinable : public HashSetBase<T> {
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  std::vector<std::unique_ptr<std::mutex>> mutexes_; //
  size_t capacity_;                 // The number of buckets
  std::atomic<size_t> size_;        // The number of elements
  std::shared_mutex resize_mutex_;

public:
  // Initialize the capacity and initialise the table
  explicit HashSetRefinable(size_t initial_capacity)
      : table_(std::vector<std::vector<T>>(initial_capacity, std::vector<T>())),
        mutexes_(std::vector<std::unique_ptr<std::mutex>>(initial_capacity)),
        capacity_(initial_capacity), size_(0) {
          for (size_t i = 0; i < mutexes_.size(); i++) {
            mutexes_[i] = std::make_unique<std::mutex>();
          }
        }

  // Add an element to the hash set
  bool Add(T elem) final {
    // Resize
    if (size_ > 4 * capacity_) {
      size_t old_capacity = capacity_;

      std::unique_lock<std::shared_mutex> rl(resize_mutex_);

      if (capacity_ == old_capacity) {
        capacity_ *= 2;

        // Resize table
        std::vector<std::vector<T>> new_table(capacity_, std::vector<T>());
        for (auto &bucket : table_) {
          for (T curr_elem : bucket) {
            size_t curr_hash = std::hash<T>()(curr_elem) % capacity_;
            new_table[curr_hash].push_back(curr_elem);
          }
        }
        table_ = new_table;
        
        // Resize locks
        for (size_t i = mutexes_.size(); i < capacity_; i++) {
          mutexes_.push_back(std::make_unique<std::mutex>());
        }
      }

    }

    // Acquire the correct mutex using a scoped lock
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);
    std::scoped_lock<std::mutex> lock(*mutexes_[std::hash<T>()(elem) % capacity_]);

    size_t hash = std::hash<T>()(elem) % capacity_;

    // If the element is already contained, return false.
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it != table_[hash].end()) {
      return false;
    }
    // Add element to the correct bucket
    table_[hash].push_back(elem);
    size_.fetch_add(1);

    // Return true for successful operation
    return true;
  }

  // Remove an element from the hashset
  bool Remove(T elem) final {
    // Acquire the correct mutex using a scoped lock
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);
    std::scoped_lock<std::mutex> lock(*mutexes_[std::hash<T>()(elem) % capacity_]);

    size_t hash = std::hash<T>()(elem) % capacity_;

    // If the element is not included, return false
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
      return false;
    }

    // Erase the element
    table_[hash].erase(it);
    size_.fetch_sub(1);
    return true;
  }

  // Check if an element is contained in the hashset
  [[nodiscard]] bool Contains(T elem) final {
    // Acquire the correct mutex using a scoped lock
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);
    std::scoped_lock<std::mutex> lock(*mutexes_[std::hash<T>()(elem) % capacity_]);

    size_t hash = std::hash<T>()(elem) % capacity_;

    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    // Return if the element was found
    return it != table_[hash].end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_.load(); }
};

#endif // HASH_SET_REFINABLE_H
