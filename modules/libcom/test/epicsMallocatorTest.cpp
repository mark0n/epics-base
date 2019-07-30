#include "epicsMallocator.hpp"
#include "epicsUnitTest.h"
#include "testMain.h"

#include <limits>

using namespace epics::allocator;

MAIN(epicsMallocatorTest)
{
    testPlan(8);

    mallocator m;

    {
        testDiag("Allocating a block of size 0");
        block b = m.allocate(0);
        testOk(b.ptr() == NULL, "results in a block containing a null pointer");
        testOk(b.size() == 0, "results in a block of size 0");
        m.deallocate(b);
    }

    {
        testDiag("Allocating a block of size 42");
        std::size_t arbitrarySize = 42;
        block b = m.allocate(arbitrarySize);
        testOk(b.ptr() != NULL, "results in a block containing a non-null pointer");
        testOk(b.size() == arbitrarySize, "results in a block of size %zu", arbitrarySize);
        m.deallocate(b);
    }

    {
        testDiag("Allocating a too large block");
        block b = m.allocate(std::numeric_limits<std::size_t>::max());
        testOk(b.ptr() == NULL, "results in a block containing a null pointer (pointer is %p)", b.ptr());
        testOk(b.size() == 0, "results in a block of size 0 (size is %zu", b.size());
        m.deallocate(b);
    }

    {
        testDiag("Mallocator instances");
        testOk(m == m, "compare equal to themselves (reflexivity)");
        mallocator m2;
        std::size_t arbitrarySize = 42;
        block b = m.allocate(arbitrarySize);
        m2.deallocate(b);
        testOk(m == m2, "can be used interchangeably (any two instances compare equal)");
    }

    return testDone();
}
