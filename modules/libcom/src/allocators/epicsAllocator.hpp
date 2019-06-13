#ifndef EPICSALLOCATOR_HPP
#define EPICSALLOCATOR_HPP

#include <utility> // std::size_t
#include <cstddef> // NULL

namespace epics {
  namespace allocator {
    class block {
      void *ptr_;
      std::size_t size_;
    public:
      block() : ptr_(NULL), size_(0) {}
      block(void *ptr, std::size_t size) : ptr_(ptr), size_(size) {}
      block(const block &x) : ptr_(x.ptr()), size_(x.size()) {}
      ~block() {}

      void * ptr() const { return ptr_; }
      std::size_t size() const { return size_; }
    };

    inline bool operator==(const block& lhs, const block& rhs) {
      return lhs.ptr() == rhs.ptr() && lhs.size() == rhs.size();
    }

    inline bool operator!=(const block& lhs, const block& rhs) {
      return lhs.ptr() != rhs.ptr() || lhs.size() != rhs.size();
    }

    struct base {
      virtual block allocate(std::size_t size) = 0;
      virtual void deallocate(block& b) = 0;
    };
  }
}

#endif // EPICSALLOCATOR_HPP
