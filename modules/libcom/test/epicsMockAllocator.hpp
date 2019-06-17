#include "epicsAllocator.hpp"
#include "epicsUnitTest.h"

// A simple class that mocks an EPICS allocator. It's destructor checks if
// allocate/deallocate have been called the right number of times.

namespace epics {
  namespace allocator {
    template <int expectedAllocCalls, int expectedDeallocCalls, class Allocator>
    class mock : public base {
      typename Allocator::value_type &parent_;
      int allocCalls_;
      int deallocCalls_;
    public:
      mock() :
          parent_(Allocator::instance()),
          allocCalls_(0),
          deallocCalls_(0) {};
      ~mock() {
        testOk(allocCalls_ == expectedAllocCalls, "calls allocate %d times", expectedAllocCalls);
        testOk(deallocCalls_ == expectedDeallocCalls, "calls deallocate %d times", expectedDeallocCalls);
      }

      block allocate(std::size_t size) {
        allocCalls_++;
        return parent_.allocate(size);
      }

      void deallocate(block& b) {
        deallocCalls_++;
        parent_.deallocate(b);
      }
    };
  }
}