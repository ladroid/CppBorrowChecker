#include <cassert>
#include <exception>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

enum class BorrowState { Valid, Invalid, MutableBorrowed };

class BorrowChecker {
private:
  std::unordered_map<void *, BorrowState> borrow_map_;

public:
  BorrowChecker() = default;

  void add_borrow(void *ptr, BorrowState state) {
    assert(borrow_map_.count(ptr) == 0); // make sure the key doesn't already exist
    borrow_map_[ptr] = state;
  }

  void remove_borrow(void *ptr) {
    assert(borrow_map_.count(ptr) != 0); // make sure the key exists
    borrow_map_.erase(ptr);
  }

  BorrowState check_borrow(void *ptr) {
    auto it = borrow_map_.find(ptr);
    if (it == borrow_map_.end()) {
      return BorrowState::Valid;
    }
    return it->second;
  }
};

template <typename T> class Ref {
private:
  T *data_;
  BorrowChecker *borrow_checker_;

public:
  Ref(T *data, BorrowChecker *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ != nullptr); // make sure borrow_checker is not nullptr
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
  }

  Ref(Ref<T> &&other) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ != nullptr); // make sure borrow_checker is not nullptr
    if (borrow_checker_->check_borrow(other.data_) != BorrowState::Valid) {
      throw std::runtime_error("cannot move value while it is borrowed");
    }
    data_ = other.data_;
    borrow_checker_ = other.borrow_checker_;
    borrow_checker_->remove_borrow(other.data_);
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
  }

  Ref<T> &operator=(Ref<T> &&other) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ != nullptr); // make sure borrow_checker is not nullptr
    if (this == &other) {
      throw std::runtime_error("cannot move the same value twice");
    }
    if (borrow_checker_->check_borrow(other.data_) != BorrowState::Valid) {
      throw std::runtime_error("cannot move value while it is borrowed");
    }
    borrow_checker_->remove_borrow(data_);
    data_ = other.data_;
    borrow_checker_ = other.borrow_checker_;
    borrow_checker_->remove_borrow(other.data_);
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
    return *this;
  }

  Ref(const Ref<T> &other)
      : data_(other.data_), borrow_checker_(other.borrow_checker_) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ != nullptr); // make sure borrow_checker is not nullptr
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
  }

  Ref<T> &operator=(const Ref<T> &other) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ != nullptr); // make sure borrow_checker is not nullptr
    if (this != &other) {
      borrow_checker_->remove_borrow(data_);
      data_ = other.data_;
      borrow_checker_ = other.borrow_checker_;
      borrow_checker_->add_borrow(data_, BorrowState::Valid);
    }
    return *this;
  }

  ~Ref() { borrow_checker_->remove_borrow(data_); }

  T *operator->() {
    if (data_ == nullptr) {
      throw std::runtime_error("Ref is empty");
    }
    return data_;
  }

  T &operator*() {
    if (data_ == nullptr) {
      throw std::runtime_error("Ref is empty");
    }
    return *data_;
  }

  explicit operator bool() const { return data_ != nullptr; }
};

template <typename T> class MutableRef {
private:
  T *data_;
  BorrowChecker *borrow_checker_;

public:
  MutableRef(T *data, BorrowChecker *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker) {
    if (borrow_checker_->check_borrow(data_) != BorrowState::Valid) {
      throw std::runtime_error(
          "cannot borrow as mutable more than once, already borrowed");
    }
    borrow_checker_->add_borrow(data_, BorrowState::MutableBorrowed);
  }

  MutableRef(const MutableRef<T> &other) = delete;

  MutableRef<T> &operator=(const MutableRef<T> &other) = delete;

  ~MutableRef() { borrow_checker_->remove_borrow(data_); }

  T *operator->() { return data_; }

  T &operator*() { return *data_; }
};

void start_v1() {
  BorrowChecker borrow_checker;
  std::vector<int> data = {1, 2, 3, 4, 5};
  std::vector<int> dat = {1, 2, 3, 4, 5};

  // Borrowing as immutable
  {
    Ref<std::vector<int>> data_ref(&data, &borrow_checker);
    std::cout << "Data (immutable):";
    for (const auto &item : *data_ref) {
      std::cout << " " << item;
    }
    std::cout << std::endl;
  }

  // Borrowing as mutable
  {
    MutableRef<std::vector<int>> data_mut_ref(&dat, &borrow_checker);
    std::cout << "Data (mutable):";
    for (auto &item : *data_mut_ref) {
      item *= 2;
      std::cout << " " << item;
    }
    std::cout << std::endl;
    try {
      MutableRef<std::vector<int>> data_mut_ref(&dat, &borrow_checker);
    } catch (const std::runtime_error &e) {
      std::cout << "Error: " << e.what() << std::endl;
    }
  }
}