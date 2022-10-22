#ifndef HASH_SET_REFINABLE_H
#define HASH_SET_REFINABLE_H

#include "src/hash_set_base.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

template <typename T> class HashSetRefinable : public HashSetBase<T> {
private:
  std::vector<std::vector<T>> table_; // A vector of vectors for storage
  std::vector<std::unique_ptr<std::mutex>>
      mutexes_;                    // Resizable vector of mutexes
  std::shared_mutex resize_mutex_; // Shared mutex for resizing
  size_t capacity_;                // The number of buckets
  std::atomic<size_t> size_;       // The number of elements

  // We have a vector of unique pointers to allow for the resizing
  // of the mutexes.
  //
  // When resizing, we do not want anything else to happen, but
  // when adding or removing elements, we want to do them at the
  // same time. This is similar to the behaviour of a shared mutex.
  // When resizing or (writing), we want no operations (readers)
  // to have access to the data, nor other writers.
  // When doing an operation (reading), we want other operations
  // to take place, but no resizing (writing).
  //
  // This is a convenient data structure for the job.

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
    // If the average bucket size is 4, increase size.
    // Resizing happens at the beginning of Add, so we don't have to
    // explicitly drop the lock acquired during add.
    if (size_ > 4 * capacity_) {
      resize();
    }

    // Get the resize lock in read mode
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);

    //  Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> lock(*mutexes_[hash % capacity_]);

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
    // Get the resize lock in read mode
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);

    // Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem) % capacity_;
    std::scoped_lock<std::mutex> lock(*mutexes_[hash % capacity_]);

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
    // Get the resize lock in read mode
    std::shared_lock<std::shared_mutex> rl(resize_mutex_);

    // Acquire the correct mutex using a scoped lock
    size_t hash = std::hash<T>()(elem);
    std::scoped_lock<std::mutex> lock(*mutexes_[hash % capacity_]);

    // Find the element
    std::vector<T> &bucket = table_[hash % capacity_];
    auto it = std::find(bucket.begin(), bucket.end(), elem);

    // Return if the element was found
    return it != bucket.end();
  }

  // Get the size of the hashset
  [[nodiscard]] size_t Size() const final { return size_.load(); }

private:
  void resize() {
    size_t old_capacity = capacity_;
    std::unique_lock<std::shared_mutex> rl(resize_mutex_);

    // If someone already resized, return
    if (capacity_ != old_capacity) {
      return;
    }
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
};

#endif // HASH_SET_REFINABLE_H
