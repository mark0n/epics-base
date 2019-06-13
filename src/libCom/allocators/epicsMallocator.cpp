#include <cstdlib> // std::malloc, std::free, NULL
#include <iostream>

#include "epicsMallocator.hpp"

namespace epics {
  namespace allocator {
    block mallocator::allocate(std::size_t size) {
      std::cerr << "epics::allocator::mallocator: allocating " << size << " bytes\n";
      if (size == 0) {
        return block(NULL, 0);
      }

      void *p = std::malloc(size);
      block result;
      if (p != NULL) {
        result = block(p, size);
      }
      return result;
    }

    void mallocator::deallocate(block& b) {
      std::cerr << "epics::allocator::mallocator: deallocating " << b.size() << " bytes\n";
      if (b.ptr()) {
        std::free(b.ptr());
        b = block(NULL, 0);
      }
    }
  }
}