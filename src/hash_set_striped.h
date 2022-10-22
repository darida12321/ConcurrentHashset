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
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  std::vector<std::mutex> mutexes_;
  size_t capacity_;          // The number of buckets
  std::atomic<size_t> size_; // The number of elements

public:
  // Initialize the capacity and initialise the table
  explicit HashSetStriped(size_t initial_capacity)
      : table_(std::vector<std::vector<T>>(initial_capacity, std::vector<T>())),
        mutexes_(std::vector<std::mutex>(initial_capacity)),
        capacity_(initial_capacity), size_(0) {}

  // Add an element to the hash set
  bool Add(T elem) final {
    // If the average bucket size is 4, increase size.
    // No need for double checking for the size changing,
    // since we still have the lock
    if (size_ > 4 * capacity_) {
      size_t old_capacity = capacity_;

      // Acquire all of the locks except the one we hold
      for (size_t i = 0; i < mutexes_.size(); i++) {
        mutexes_[i].lock();
      }

      if (capacity_ == old_capacity) {
        capacity_ *= 2;
        // Create a new, bigger table
        std::vector<std::vector<T>> new_table(capacity_, std::vector<T>());
        // Move all old table elements to new one
        for (auto &bucket : table_) {
          for (T curr_elem : bucket) {
            size_t curr_hash = std::hash<T>()(curr_elem) % capacity_;
            new_table[curr_hash].push_back(curr_elem);
          }
        }
        table_ = new_table;
      }

      // Set old table to the new one
      for (size_t i = 0; i < mutexes_.size(); i++) {
        mutexes_[i].unlock();
      }
    }

    size_t hash = std::hash<T>()(elem) % capacity_;
    // Acquire the correct mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutexes_.size()]);
    hash = std::hash<T>()(elem) % capacity_;

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
    size_t hash = std::hash<T>()(elem) % capacity_;
    // Acquire the correct mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutexes_.size()]);

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
    size_t hash = std::hash<T>()(elem) % capacity_;
    // Acquire the correct mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutexes_.size()]);

    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    // Return if the element was found
    return it != table_[hash].end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_.load(); }
};

#endif // HASH_SET_STRIPED_H
