#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <stack>
#include <cassert>

// Comment the following line,
// if you want to disable safe mode
#define STACK_SAFE_MODE

// Comment the following line,
// if you want to disable dumps while handling errors
#define STACK_ENABLE_DUMP


// Implementation

#ifdef  STACK_SAFE_MODE
#define CHECK_VALIDITY check_validity();
#else
#define CHECK_VALIDITY ;
#endif

#define has_error(error_type) ((errors_ & (error_type)) != 0)
#define set_error(error_type) errors_ |= (error_type);

#define POISON 0xDEADBEEF

enum StackErrors {
  POP_FROM_EMPTY_STACK    = (1 << 1),
  BAD_ALLOC               = (1 << 2),
  WRONG_CONTROL_SUM       = (1 << 3),
  CANARY_BEFORE_CORRUPTED = (1 << 4),
  CANARY_AFTER_CORRUPTED  = (1 << 5),
  TOP_FROM_EMPTY_STACK    = (1 << 6)
};

template <class T,
    class Hash = std::hash<T>,
    class Allocator = std::allocator<T> >
class CherepkovStack
{

  // **************
  // TYPEDEFS AND CONSTANTS
  // **************

  typedef T                                      value_type;
  typedef T*                                     pointer;
  typedef T&                                     reference;
  typedef const T&                               const_reference;
  typedef T&&                                    rvalue_reference;
  typedef Hash                                   hash_function_type;
  typedef typename std::result_of<Hash(T)>::type hash_value_type;
  typedef Allocator                              allocator_type;

  static const size_t GROWTH_FACTOR_{2};

  // *****************
  // PRIVATE METHODS
  // *****************

  void check_control_sum() const {
    hash_value_type control_sum = 0;

    for (size_t i = 0; i < size_; ++i) {
      control_sum += hash_function_(buffer_[i]);
    }

    if (control_sum_ != control_sum) {
      set_error(WRONG_CONTROL_SUM);
    }
  }

  void check_canaries() const {
    if (canary_before_ != POISON) {
      set_error(CANARY_BEFORE_CORRUPTED);
    }

    if (canary_after_ != POISON) {
      set_error(CANARY_AFTER_CORRUPTED);
    }
  }

  void check_validity() const {
    check_control_sum();
    check_canaries();
    print_errors();
  }

  void print_errors() const {
    if (errors_ == 0) {
      return;
    }

    fprintf(stderr, "Errors found:\n");
    if (has_error(POP_FROM_EMPTY_STACK)) {
      fprintf(stderr, "\tPop from empty stack was performed;\n");
    }
    if (has_error(BAD_ALLOC)) {
      fprintf(stderr, "\tFailed to allocate memory;\n");
    }
    if (has_error(WRONG_CONTROL_SUM)) {
      fprintf(stderr, "\tCheck of control sum failed;\n");
    }
    if (has_error(CANARY_BEFORE_CORRUPTED)) {
      fprintf(stderr, "\tCanary before the stack is corrupted;\n");
    }
    if (has_error(CANARY_AFTER_CORRUPTED)) {
      fprintf(stderr, "\tCanary after the stack is corrupted;\n");
    }
    if (has_error(TOP_FROM_EMPTY_STACK)) {
      fprintf(stderr, "\tTop from empty stack was performed;\n");
    }

#ifdef STACK_ENABLE_DUMP
    dump();
#endif STACK_ENABLE_DUMP

    throw std::runtime_error("Stack exception");
  }

#ifdef STACK_ENABLE_DUMP
  void dump() const {
    if (buffer_ == nullptr)
      return;

    fprintf(stderr, "Dump:\nstack = %p\n{\n", (void*)this);
    fprintf(stderr, "\tbuffer[%zu] = %p\n\t{\n", capacity_, (void*)buffer_);
    for (size_t i = 0; i < capacity_; ++i) {
      std::cerr << "\t\t[" << i << "] = " << buffer_[i] << "\n";
    }
    fprintf(stderr, "\t}\n\tsize = %zu\n}\n", size_);
  }
#endif

  void expand() {
    assert(size_ == capacity_);

    size_t new_capacity = size_ * GROWTH_FACTOR_;
    pointer new_buffer;

    try {
      new_buffer = allocator_.allocate(new_capacity);
    } catch(const std::bad_alloc& e) {
      set_error(BAD_ALLOC);
      return;
    }

    std::copy(buffer_, buffer_ + capacity_, new_buffer);

    allocator_.deallocate(buffer_, capacity_);

    std::swap(buffer_, new_buffer);
    capacity_ = new_capacity;
  }

 public:

  // *****************
  // CONSTRUCTORS
  // DESTRUCTOR,
  // ASSIGNMENT OPERATORS
  // *****************

  explicit CherepkovStack(size_t capacity = 64,
                          const hash_function_type& hash_function = hash_function_type(),
                          const allocator_type&     allocator     = allocator_type()) :
      capacity_(capacity),
      size_(0),
      hash_function_(hash_function),
      allocator_(allocator)
  {
    try {
      buffer_ = allocator_.allocate(capacity_);
    } catch (const std::bad_alloc&) {
      set_error(BAD_ALLOC);
    }
    CHECK_VALIDITY;
  }

  CherepkovStack(const CherepkovStack&) = delete;
  CherepkovStack(CherepkovStack&&)      = delete;

  CherepkovStack& operator =(const CherepkovStack&) = delete;
  CherepkovStack& operator =(CherepkovStack&&)      = delete;

  ~CherepkovStack() {
    allocator_.deallocate(buffer_, capacity_);
  }

  // *****************
  // PUBLIC METHODS
  // *****************

  bool empty() {
    CHECK_VALIDITY;
    return size_ == 0;
  }

  void pop() {
    if (size_ == 0) {
      set_error(POP_FROM_EMPTY_STACK);
    } else {
      control_sum_ -= hash_function_(buffer_[size_ - 1]);
      --size_;
    }
    CHECK_VALIDITY;
  }

  reference top() {
    return const_cast<reference>(static_cast<const CherepkovStack*>(this)->top());
  }

  const_reference top() const {
    if (size_ == 0) {
      set_error(TOP_FROM_EMPTY_STACK);
    }
    CHECK_VALIDITY;
    return buffer_[size_ - 1];
  }

  void push(const_reference element) {
    if (size_ == capacity_) {
      expand();
    }
    CHECK_VALIDITY;
    assert(size_ < capacity_);

    buffer_[size_] = element;
    ++size_;
    control_sum_ += hash_function_(element);
  }

  void push(rvalue_reference element) {
    if (size_ == capacity_) {
      expand();
    }
    CHECK_VALIDITY;
    assert(size_ < capacity_);

    control_sum_ += hash_function_(element);
    buffer_[size_] = std::move(element);
    ++size_;
  }

 private:

  // *****************
  // PRIVATE MEMBERS
  // *****************

  size_t                  canary_before_{POISON};

  hash_function_type       hash_function_;
  hash_value_type          control_sum_{0};

  size_t                   capacity_;
  size_t                   size_;

  pointer                  buffer_{nullptr};

  allocator_type           allocator_;

  mutable int              errors_{0};

  size_t                  canary_after_{POISON};

};

int main() {
  CherepkovStack<std::string> s(4);
  s.push("kek");
  s.push("kek");
  return 0;
}
