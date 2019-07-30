#ifndef EPICSMALLOCATOR_HPP
#define EPICSMALLOCATOR_HPP

#include "epicsAllocator.hpp"

#include "cpp11.h"

namespace epics {
  namespace allocator {
    struct mallocator final : public base {
      block allocate(std::size_t size) override final;
      void deallocate(block& b) override final;
    };

    bool operator==(const mallocator&, const mallocator&);
    bool operator!=(const mallocator&, const mallocator&);
  }
}

#endif // EPICSMALLOCATOR_HPP
