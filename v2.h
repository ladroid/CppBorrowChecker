#include <cassert>
#include <exception>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

enum class BorrowState { Valid, Invalid, MutableBorrowed, Owned };

class BorrowChecker {
private:
  std::unordered_map<void *, BorrowState> borrow_map_;

public:
  BorrowChecker() = default;

  void add_borrow(void *ptr, BorrowState state) {
    assert(borrow_map_.count(ptr) ==
           0); // make sure the key doesn't already exist
    borrow_map_[ptr] = state;
  }

  void remove_borrow(void *ptr) {
    auto it = borrow_map_.find(ptr);
    if (it == borrow_map_.end()) {
      // The key is not present in the map
      return;
    }
    borrow_map_.erase(it);
  }

  BorrowState check_borrow(void *ptr) {
    auto it = borrow_map_.find(ptr);
    if (it == borrow_map_.end()) {
      return BorrowState::Valid;
    }
    return it->second;
  }

  void set_owned(void *ptr) {
    auto it = borrow_map_.find(ptr);
    if (it != borrow_map_.end()) {
      it->second = BorrowState::Owned;
    }
  }

  bool check_owned(void *ptr) {
    auto it = borrow_map_.find(ptr);
    if (it == borrow_map_.end()) {
      return false;
    }
    return it->second == BorrowState::Owned;
  }
};

template <typename T> class Own {
private:
  T *data_;
  BorrowChecker *borrow_checker_;
  bool is_owner_;

public:
  explicit Own(T *data, BorrowChecker *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker), is_owner_(true) {}

  Own(const Own &) = delete;
  Own &operator=(const Own &) = delete;

  Own(Own &&other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        borrow_checker_(other.borrow_checker_), is_owner_(other.is_owner_) {
    other.is_owner_ = false;
  }

  Own &operator=(Own &&other) noexcept {
    if (this != &other) {
      borrow_checker_->remove_borrow(data_);
      data_ = std::exchange(other.data_, nullptr);
      borrow_checker_ = other.borrow_checker_;
      is_owner_ = other.is_owner_;
      other.is_owner_ = false;
    }
    return *this;
  }

  T *get() const { return data_; }

  ~Own() {
    if (is_owner_) {
      borrow_checker_->remove_borrow(data_);
      delete data_;
    }
  }

  T *operator->() const { return data_; }

  T &operator*() const { return *data_; }

  void set_owner() {
    if (!is_owner_) {
      throw std::runtime_error("value already has an owner");
    }
    borrow_checker_->set_owned(data_);
  }

  bool is_owned() const { return borrow_checker_->check_owned(data_); }
};

template <typename T> class Ref {
private:
  T *data_;
  BorrowChecker *borrow_checker_;

public:
  Ref(T *data, BorrowChecker *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker) {
    assert(data_ != nullptr); // make sure data is not nullptr
    assert(borrow_checker_ !=
           nullptr); // make sure borrow_checker is not nullptr
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
  }

  Ref(const Ref &) = delete;
  Ref &operator=(const Ref &) = delete;

  Ref(Ref &&other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        borrow_checker_(other.borrow_checker_) {}

  Ref &operator=(Ref &&other) noexcept {
    if (this != &other) {
      if (borrow_checker_->check_borrow(other.data_) != BorrowState::Valid) {
        throw std::runtime_error("cannot move value while it is borrowed");
      }
      borrow_checker_->remove_borrow(data_);
      data_ = std::exchange(other.data_, nullptr);
      borrow_checker_ = other.borrow_checker_;
      borrow_checker_->add_borrow(data_, BorrowState::Valid);
    }
    return *this;
  }

  ~Ref() {
    if (data_ != nullptr) {
      borrow_checker_->remove_borrow(data_);
    }
  }

  T *operator->() const { return data_; }

  T &operator*() const { return *data_; }
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

void start_v2() {
  BorrowChecker borrow_checker;
  // Borrowing as immutable
  Own<int> my_int(new int(42), &borrow_checker);
  Ref<int> data_ref(my_int.get(), &borrow_checker);
  std::cout << "Data (immutable):" << *data_ref << "\n";

  // Borrowing as mutable
  {
    Own<int> my_int(new int(42), &borrow_checker);
    MutableRef<int> data_mut_ref(my_int.get(), &borrow_checker);
    std::cout << "Data (mutable):" << *data_mut_ref << "\n";
    try {
      MutableRef<int> data_mut_ref(my_int.get(), &borrow_checker);
    } catch (const std::runtime_error &e) {
      std::cout << "Error: " << e.what() << "\n";
    }
  }

  {
    Own<int> my_int(new int(42), &borrow_checker);
    {
      Own<int> my_int2(std::move(my_int));
      Ref<int> my_ref2(my_int2.get(), &borrow_checker);
      try {
        std::cout << "res: " << *my_ref2 << std::endl;
      } catch (const std::runtime_error &e) {
        std::cerr << e.what();
      }
    }
    try {
      Ref<int> my_ref(my_int.get(), &borrow_checker);
    } catch (const std::runtime_error &e) {
      std::cerr << e.what();
    }
  }
}