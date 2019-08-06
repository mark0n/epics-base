#include "epicsUnitTest.h"
#include "testMain.h"

#include <vector>

#include "epicsSTLAllocator.hpp"
#include "epicsMallocator.hpp"

using namespace epics::allocator;

MAIN(stlAllocatorTest)
{
    testPlan(11);

    testDiag("Allocating a std::vector using epicsSTLAllocator backed by epicsMallocator");

    typedef stl<int, mallocator> stlAllocator;

    const int numbers[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    size_t arraysize = sizeof(numbers) / sizeof(numbers[0]);

    std::vector<int, stlAllocator> v;
    for (size_t i = 0; i < arraysize; ++i) {
        v.push_back(numbers[i]);
    }

    testOk(v.size() == arraysize, "vector size %zu, number of pushed elements %zu", v.size(), arraysize);

    for (size_t i = 0; i < v.size(); ++i) {
        testOk(v[i] == numbers[i], "compare vector element %zu: pushed %d, read back: %d", i, numbers[i], v[i]);
    }

    return testDone();
}
