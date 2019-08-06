#include "epicsFreeListAllocator.hpp"
#include "epicsMallocator.hpp"
#include "epicsMockAllocator.hpp"
#include "epicsUnitTest.h"
#include "testMain.h"

#include <vector>
#include <algorithm>

using namespace epics::allocator;

MAIN(epicsFreeListAllocatorTest)
{
    testPlan(29);

    const int arbitrarySize = 64;
    typedef freeList<arbitrarySize, mallocator> fla;

    {
        testDiag("Allocating a single block");
        fla fl;
        block b = fl.allocate(arbitrarySize);
        testOk(b.ptr() != NULL, "results in a block containing a non-null pointer");
        testOk(b.size() == arbitrarySize, "results in a block of the requested size");
        fl.deallocate(b);
    }

    {
        testDiag("Allocating multiple blocks");
        fla fl;
        std::vector<block> vb;
        for (int i = 0; i < 10; i++) {
          block b = fl.allocate(arbitrarySize);
          testOk(b.ptr() != NULL, "results in a block containing a non-null pointer");
          testOk(b.size() == arbitrarySize, "results in a block of the requested size");
          vb.push_back(b);
        }
        for (int i = 0; i < 10; i++) {
          fl.deallocate(vb[i]);
        }
    }

    {
        testDiag("Allocating blocks that have been freed before");
        fla fl;
        std::vector<block> vb;
        for (int i = 0; i < 3; i++) {
          vb.push_back(fl.allocate(arbitrarySize));
        }
        for (int i = 0; i < 3; i++) {
          fl.deallocate(vb[i]);
        }

        for (int i = 0; i < 3; i++) {
          block newBlock = fl.allocate(arbitrarySize);
          std::vector<block>::iterator it;
          it = std::find(vb.begin(), vb.end(), newBlock);
          testOk(newBlock == *it, "results in the blocks being reused");
          vb.erase(it);
        }

        for (int i = 0; i < 3; i++) {
          fl.deallocate(vb[i]);
        }
    }

    {
        testDiag("Allocating/deallocating blocks with sizes handled by parent allocator");
        typedef freeList<arbitrarySize,
                mock<1, 1,
                mallocator>> fla;
        fla fl;
        block firstBlock = fl.allocate(2 * arbitrarySize);
        testOk(firstBlock.ptr() != NULL, "results in a block containing a non-null pointer");
        testOk(firstBlock.size() == 2 * arbitrarySize, "results in a block of the requested size");
        fl.deallocate(firstBlock);
        //TODO: modify global allocator such that it destroys the allocator when the last instance is being destroyed
    }

    // test allocating blocks smaller than a pointer -> size needs to be increased automatically (or allocation needs to fail)
    return testDone();
}
