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

// This is a RAII lock to acquire an array of mutexes in order
// This makes the resizing easier to implement and safer
class ArrayLock {
private:
  std::mutex *mutexes_;
  size_t size_;

public:
  ArrayLock(std::mutex *mutexes, size_t size) : mutexes_(mutexes), size_(size) {
    for (size_t i = 0; i < size_; i++) {
      mutexes_[i].lock();
    }
  }

  ~ArrayLock() {
    for (size_t i = 0; i < size_; i++) {
      mutexes_[i].unlock();
    }
  }
};

template <typename T> class HashSetStriped : public HashSetBase<T> {
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  std::mutex *mutexes_;               // An array of mutexes
  size_t mutex_count_;                // The number of elements in the array
  size_t capacity_;                   // The number of buckets
  std::atomic<size_t> size_;          // The number of elements

  // Mutexes is a constant sized array. When acquireing it all, we can
  // use the ArrayLock for a RAII locking mechanism.
  //
  // Size has to be an atomic variable, because multiple threads can
  // modify it at the same time.
  //
  // Capacity is only changed when resizing, which is done by one
  // thread at a time, so it is a normal variable.

public:
  // Initialize the capacity and initialise the table
  explicit HashSetStriped(size_t initial_capacity)
      : table_(std::vector<std::vector<T>>(initial_capacity, std::vector<T>())),
        mutexes_(new std::mutex[initial_capacity]),
        mutex_count_(initial_capacity), capacity_(initial_capacity), size_(0) {}

  ~HashSetStriped() override { delete[] mutexes_; }

  // Add an element to the hash set
  bool Add(T elem) final {
    // If the average bucket size is 4, increase size.
    // Resizing happens at the beginning of Add, so we don't have to
    // explicitly drop the lock acquired during add.
    if (size_ > 4 * capacity_) {
      resize();
    }

    //  Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutex_count_]);

    // If the element is already contained, return false.
    std::vector<T> &bucket = table_[hash % capacity_];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it != bucket.end()) {
      return false;
    }

    // Add element to the correct bucket
    bucket.push_back(elem);
    size_.fetch_add(1);

    // Return true for successful operation
    return true;
  }

  // Remove an element from the hashset
  bool Remove(T elem) final {
    // Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutex_count_]);

    // If the element is not included, return false
    std::vector<T> &bucket = table_[hash % capacity_];
    auto it = std::find(bucket.begin(), bucket.end(), elem);
    if (it == bucket.end()) {
      return false;
    }

    // Erase the element
    bucket.erase(it);
    size_.fetch_sub(1);
    return true;
  }

  // Check if an element is contained in the hashset
  [[nodiscard]] bool Contains(T elem) final {
    // Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> lock(mutexes_[hash % mutex_count_]);

    // Find the element
    std::vector<T> &bucket = table_[hash % capacity_];
    auto it = std::find(bucket.begin(), bucket.end(), elem);

    // Return if the element was found
    return it != bucket.end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_.load(); }

private:
  // Double the size of the hashset
  void resize() {
    size_t old_capacity = capacity_;

    // Acquire all of the locks except the one we hold
    ArrayLock al(mutexes_, mutex_count_);

    // Check if someone else has already resized
    if (capacity_ != old_capacity) {
      return;
    }
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
};

#endif // HASH_SET_STRIPED_H
