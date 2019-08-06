#ifndef EPICSFREELISTALLOCATOR_HPP
#define EPICSFREELISTALLOCATOR_HPP

#include "epicsAllocator.hpp"
#include "compilerSpecific.h"
#include <iostream>
#include "cpp11.h"

// A simple free list for blocks that are all exactly the same size. There is
// no limit on the number of elements in the list. This free list is NOT thread
// safe.

namespace epics {
  namespace allocator {
    template <std::size_t blockSize, class Allocator>
    class freeList : public base {
      static_assert(blockSize >= sizeof(void *), "epics::allocator::freeList doesn't support block sizes smaller than the size of a pointer");
      Allocator parent_;
      struct Node { Node *next; } *root_;
    public:
      freeList() : root_(NULL) {};
      ~freeList() { deallocateAll(); }
      block allocate(std::size_t size) override final;
      void deallocate(block& b) override final;
      void deallocateAll();
    };

    template <std::size_t blockSize, class Allocator>
    block freeList<blockSize, Allocator>::allocate(std::size_t size) {
      if (size == blockSize && root_) {
        std::cerr << "epics::allocator::freeList: returning a block of size " << size << " bytes from free list\n";
        block b = block(root_, size);
        root_ = root_->next;
        return b;
      }
      std::cerr << "epics::allocator::freeList: allocating " << size << " bytes with parent allocator\n";
      return parent_.allocate(size);
    }

    template <std::size_t blockSize, class Allocator>
    void freeList<blockSize, Allocator>::deallocate(block& b) {
      if (b.size() != blockSize) {
        std::cerr << "epics::allocator::freeList: delegating deallocation of " << b.size() << " bytes to parent allocator\n";
        parent_.deallocate(b);
        return;
      }
      std::cerr << "epics::allocator::freeList: putting a block of " << b.size() << " bytes into the free list\n";
      Node *p = static_cast<Node *>(b.ptr());
      p->next = root_;
      root_ = p;
    }

    template <std::size_t blockSize, class Allocator>
    void freeList<blockSize, Allocator>::deallocateAll() {
      std::cerr << "epics::allocator::freeList: deallocating all elements of free list\n";
      while (root_) {
        block b = block(root_, blockSize);
        std::cerr << "epics::allocator::freeList: delegating deallocation of " << b.size() << " bytes to parent allocator\n";
        root_ = root_->next;
        parent_.deallocate(b);
      }
    }

    template <std::size_t blockSize, class Allocator>
    bool operator==(
        const freeList<blockSize, Allocator>& lhs,
        const freeList<blockSize, Allocator>& rhs)
    {
      return lhs.root_ == rhs.root_ &&
             lhs.parent_ == rhs.parent_;
    }

    template <std::size_t blockSize, class Allocator>
    bool operator!=(
        const freeList<blockSize, Allocator>& lhs,
        const freeList<blockSize, Allocator>& rhs)
    {
      return !(lhs == rhs);
    }
  }
}

#endif // EPICSFREELISTALLOCATOR_HPP
