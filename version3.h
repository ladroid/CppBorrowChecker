#include <array>
#include <type_traits>
#include <utility>
#include <tuple>
#include <stdexcept>
#include <memory>
#include <iostream>

enum class BorrowState { Valid, Invalid, MutableBorrowed, Owned };

template <std::size_t Size>
struct BorrowCheckerStates {
    std::array<BorrowState, Size> states{};
};

template <typename T, std::size_t N>
class BorrowChecker {
private:
  struct PtrState {
    void *ptr;
    BorrowState state;
  };

  std::array<PtrState, N> borrow_map_{};

  template <std::size_t... Is>
  constexpr BorrowState check_borrow_impl(void *ptr,
                                          std::index_sequence<Is...>) {
    BorrowCheckerStates<N> states{};
    (void)std::initializer_list<int>{(void(
        (borrow_map_[Is].ptr == nullptr
             ? states.states[Is] = BorrowState::Valid
             : (borrow_map_[Is].ptr == ptr
                    ? states.states[Is] = borrow_map_[Is].state
                    : states.states[Is] = BorrowState::Valid))),
                                      0)...};
    for (BorrowState state : states.states) {
      if (state != BorrowState::Valid) {
        return state;
      }
    }
    return BorrowState::Valid;
  }

public:
  constexpr BorrowChecker() {}

  constexpr void add_borrow(void *ptr, BorrowState state) {
    for (std::size_t i = 0; i < N; ++i) {
      if (borrow_map_[i].ptr == nullptr) {
        borrow_map_[i] = {ptr, state};
        return;
      }
    }
    __builtin_unreachable();
  }

  constexpr void remove_borrow(void *ptr) {
    for (std::size_t i = 0; i < N; ++i) {
      if (borrow_map_[i].ptr == ptr) {
        borrow_map_[i] = {nullptr, BorrowState::Valid};
        return;
      }
    }
  }

  constexpr BorrowState check_borrow(void *ptr) {
    return check_borrow_impl(ptr, std::make_index_sequence<N>{});
  }

  constexpr void set_owned(void *ptr) {
    for (std::size_t i = 0; i < N; ++i) {
      if (borrow_map_[i].ptr == ptr) {
        borrow_map_[i].state = BorrowState::Owned;
        return;
      }
    }
  }

  constexpr bool check_owned(void *ptr) {
    for (std::size_t i = 0; i < N; ++i) {
      if (borrow_map_[i].ptr == ptr) {
        return borrow_map_[i].state == BorrowState::Owned;
      }
    }
    return false;
  }
};


template <typename T, std::size_t N>
class Own {
private:
  T *data_;
  BorrowChecker<T, N> *borrow_checker_;
  bool is_owner_;

public:
  constexpr explicit Own(T *data, BorrowChecker<T, N> *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker), is_owner_(true) {}

  constexpr Own(const Own &) = delete;
  constexpr Own &operator=(const Own &) = delete;

  constexpr Own(Own &&other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        borrow_checker_(other.borrow_checker_),
        is_owner_(other.is_owner_) {
    other.is_owner_ = false;
  }

  constexpr Own &operator=(Own &&other) noexcept {
    if (this != &other) {
      borrow_checker_->remove_borrow(data_);
      data_ = std::exchange(other.data_, nullptr);
      borrow_checker_ = other.borrow_checker_;
      is_owner_ = other.is_owner_;
      other.is_owner_ = false;
    }
    return *this;
  }

  constexpr T *get() const { return data_; }

  ~Own() {
    if (is_owner_) {
      borrow_checker_->remove_borrow(data_);
      delete data_;
    }
  }

  constexpr T *operator->() const { return data_; }

  constexpr T &operator*() const { return *data_; }

  template <std::size_t M>
  constexpr Own<T, M> borrow() {
    if (borrow_checker_->check_borrow(data_) != BorrowState::Valid) {
      throw std::logic_error("borrow of already borrowed data");
    }
    borrow_checker_->add_borrow(data_, BorrowState::Owned);
    return Own<T, M>(data_, borrow_checker_);
  }

  constexpr void set_owned() {
    if (borrow_checker_->check_borrow(data_) != BorrowState::Valid) {
      throw std::logic_error("setting owned of borrowed data");
    }
    borrow_checker_->set_owned(data_);
    is_owner_ = true;
  }

  constexpr bool is_owner() const {
    return is_owner_ && borrow_checker_->check_owned(data_);
  }
};

template <typename T, size_t N>
class Ref {
private:
  T *data_;
  BorrowChecker<T, N> *borrow_checker_;

public:
  constexpr explicit Ref(T *data, BorrowChecker<T, N> *borrow_checker)
      : data_(data), borrow_checker_(borrow_checker) {
    BorrowState state = borrow_checker_->check_borrow(data_);
    if (state != BorrowState::Valid) {
      throw std::logic_error("Invalid borrow in Ref constructor");
    }
    borrow_checker_->add_borrow(data_, BorrowState::Valid);
  }

  constexpr Ref(const Ref &) = delete;
  constexpr Ref &operator=(const Ref &) = delete;

  constexpr Ref(Ref &&other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        borrow_checker_(other.borrow_checker_) {}

  constexpr Ref &operator=(Ref &&other) noexcept {
    if (this != &other) {
      borrow_checker_->remove_borrow(data_);
      data_ = std::exchange(other.data_, nullptr);
      borrow_checker_ = other.borrow_checker_;
    }
    return *this;
  }

  constexpr T &operator*() const { return *data_; }
  constexpr T *operator->() const { return data_; }

  ~Ref() { borrow_checker_->remove_borrow(data_); }
};

void start_v3() {
  // Allocate an int and create a BorrowChecker to track borrows.
  auto borrow_checker = std::make_unique<BorrowChecker<int, 3>>();
  auto ptr = new int(42);

  // Create a Ref to the int, add a borrow to the checker, and print the value.
  Ref<int, 3> ref(ptr, borrow_checker.get());
  borrow_checker->add_borrow(ptr, BorrowState::Valid);
  std::cout << "Ref value: " << *ref << "\n";

  // Create an Own to the int, add a borrow to the checker, and print the value.
  Own<int, 3> own(ptr, borrow_checker.get());
  borrow_checker->add_borrow(ptr, BorrowState::Owned);
  std::cout << "Own value: " << *own << "\n";

  // Transfer ownership of the int to a new Own and print the value.
  Own<int, 3> new_own = std::move(own);
  std::cout << "New own value: " << *new_own << "\n";

  // Attempt to create a new Ref and a new Own to the int and print the results.
  Ref<int, 3> new_ref(ptr, borrow_checker.get());
  std::cout << "New ref value: " << *new_ref << "\n";
  Own<int, 3> new_own2(ptr, borrow_checker.get());
  std::cout << "New own value: " << *new_own2 << "\n";
}