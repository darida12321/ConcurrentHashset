#ifndef HASH_SET_COARSE_GRAINED_H
#define HASH_SET_COARSE_GRAINED_H

#include <algorithm>
#include <cassert>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetCoarseGrained : public HashSetBase<T> {
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  std::mutex mutex_;                  // A coarse grained mutex
  size_t capacity_;                   // The number of buckets
  size_t size_ = 0;                   // The number of elements

  // size and capacity are only changed by one thread at a time,
  // so there is no need for atomic variables.

  // For the coarse grained lock, I am using a simple mutex.
  //
  // The book uses a recursive mutex, however we do not use
  // re-entrant methods, so we have no need for that.
  //
  // Using an std::shared_mutex so that read operations can work
  // in paralell would be good, but in practice, the overhead
  // from using a complex locking mechanism outweighs its
  // advantages. Here, we have an about constant time lookup.

public:
  // Initialize the capacity and initialise the table
  explicit HashSetCoarseGrained(size_t initial_capacity)
      : table_(std::vector<std::vector<T>>(initial_capacity, std::vector<T>())),
        capacity_(initial_capacity) {}

  // Add an element to the hash set
  bool Add(T elem) final {
    // Acquire the mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutex_);
    // std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t hash = std::hash<T>()(elem) % capacity_;

    // If the element is already contained, return false.
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it != table_[hash].end()) {
      return false;
    }

    // Add element to the correct bucket
    table_[hash].push_back(elem);
    size_++;

    // If the average bucket size is 4, increase size.
    //
    // We do not need to double check the size for change as
    // in the book, since we are still holding the one lock
    if (size_ > 4 * capacity_) {
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
      // Set old table to the new one
      table_ = new_table;
    }

    // Return true for successful operation
    return true;
  }

  // Remove an element from the hashset
  bool Remove(T elem) final {
    // Acquire the mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutex_);
    // std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t hash = std::hash<T>()(elem) % capacity_;
    // If the element is not included, return false
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    if (it == table_[hash].end()) {
      return false;
    }

    // Erase the element
    table_[hash].erase(it);
    size_--;
    return true;
  }

  // Check if an element is contained in the hashset
  [[nodiscard]] bool Contains(T elem) final {
    // Acquire the mutex using a scoped lock
    std::scoped_lock<std::mutex> lock(mutex_);
    // std::shared_lock<std::shared_mutex> lock(mutex_);

    // If we have a lot of lookups, a binary search algorithm
    // might be a little faster, however, due to resizing,
    // this will contain an average of 4 elements making it O(1).
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);

    // Return if the element was found
    return it != table_[hash].end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_; }
};

#endif // HASH_SET_COARSE_GRAINED_H
