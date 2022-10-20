#ifndef HASH_SET_SEQUENTIAL_H
#define HASH_SET_SEQUENTIAL_H

#include <algorithm>
#include <cassert>
#include <functional>
#include <vector>

#include "src/hash_set_base.h"

template <typename T> class HashSetSequential : public HashSetBase<T> {
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  size_t capacity_;                   // The number of buckets
  size_t size_ = 0;                   // The number of elements

public:
  // Initialize the capacity and initialise the table
  explicit HashSetSequential(size_t initial_capacity)
      : table_(std::vector<std::vector<T>>(initial_capacity, std::vector<T>())),
        capacity_(initial_capacity) {}

  // Add an element to the hash set
  bool Add(T elem) final {
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
    size_t hash = std::hash<T>()(elem) % capacity_;
    auto it = std::find(table_[hash].begin(), table_[hash].end(), elem);
    // Return if the element was found
    return it != table_[hash].end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_; }
};

#endif // HASH_SET_SEQUENTIAL_H
