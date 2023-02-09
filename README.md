# Simple borrow checker for C++

Rust uses a borrow checker to enforce its ownership rules and ensure that programs are memory safe. The ownership rules dictate how Rust manages memory over the stack and heap. As you write Rust programs, you'll need to use variables without changing the ownership of the associated value.

That's the reason to include in C++ borrow checker for writing safe code in C++.

## What is borrow checker?

One tool the Rust compiler uses to ensure the memory safety of a program is its borrow checker. The borrow checker ensures that an object is always in one of 3 states:

- Uniquely Owned (T). In this state, there are no outstanding references to the object. You can pass ownership of this object around.
  
- Has an Exclusive (aka Mutable) Reference (&mut T). In this state, there is a single mutable reference. You can not pass ownership of the object during the mutable reference’s lifetime, and you can’t store or duplicate the mutable reference — there can only be one.

- Has 1 or more Shared References (&T). In this state there are one or more references to the object, but they are shared references and should not mutate it in a way that would create data races/inconsistency. You still can not pass ownership of the object while these references are around.

## Completed

✅ That all variables are initialized before they are used.

✅ Can't move the same value twice

✅ Can't move a value while it is borrowed

✅ Can't access a place while it is mutably borrowed

✅ Can't mutate a place while it is immutably borrowed

✅ Work with STL

## ToDo List

❎ Check lifetime object

## Usage

```c++
int main() {
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

  return 0;
}
```