#ifndef EPICSSTLALLOCATOR_HPP
#define EPICSSTLALLOCATOR_HPP

#include <cstddef> // for std::size_t
#include <new> // for bad_alloc
#include <limits>
#include <iostream>

#include "epicsAllocator.hpp"

namespace epics {
  namespace allocator {
    template <typename T, class Allocator>
    class stl {
      Allocator allocator_;
    public:
      typedef T           value_type;
      typedef T *         pointer;
      typedef const T *   const_pointer;
      typedef T &         reference;
      typedef const T &   const_reference;
      typedef std::size_t size_type;
      typedef ptrdiff_t   difference_type;

      pointer address(reference x) const {
        return &x;
      }
      const_pointer address(const_reference x) const {
        return &x;
      }

      stl() {};
      stl(const stl&) {};
      template <typename U>
      explicit stl(const stl<U, Allocator> &) {}
      ~stl() {}
      // Note: We don't define the assignment operator (we don't allow overwriting of the allocator after memory has been allocated).

      template <typename U>
      struct rebind {
        typedef stl<U, Allocator> other;
      };

      void construct(pointer p, const value_type& x) {
        new(p) value_type(x);
      }
      void destroy(pointer p) {
        p->~value_type();
      }

      size_type max_size() const {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
      }

      pointer allocate(size_type n, const_pointer /* hint */ = NULL) {
        std::cerr << "epics::allocator::stl: allocating " << n << " objects of size " << sizeof(value_type) << "\n";
        block b = allocator_.allocate(n * sizeof(value_type));
        if (b.ptr()) {
          return static_cast<pointer>(b.ptr());
        }
        throw std::bad_alloc();
      }

      void deallocate(pointer p, size_type n) {
        std::cerr << "epics::allocator::stl: deallocating " << n << " objects of size " << sizeof(value_type) << "\n";
        block b(p, n * sizeof(value_type));
        allocator_.deallocate(b);
      }
    };

    template <typename T, class Allocator>
    bool operator==(
        const stl<T, Allocator>& lhs,
        const stl<T, Allocator>& rhs)
    {
      return lhs.allocator_ == rhs.allocator_;
    }

    template <typename T, class Allocator>
    bool operator!=(
        const stl<T, Allocator>& lhs,
        const stl<T, Allocator>& rhs)
    {
      return !(lhs == rhs);
    }
  }
}

#endif // EPICSSTLALLOCATOR_HPP
