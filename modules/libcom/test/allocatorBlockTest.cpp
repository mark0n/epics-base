#include "epicsAllocator.hpp"
#include "epicsUnitTest.h"
#include "testMain.h"

using epics::allocator::block;

MAIN(blockTest)
{
  testPlan(12);

  int i, j;

  testDiag("A memory block object");
  {
    block b;
    testOk(b.ptr() == NULL, "by default contains a nullptr");
    testOk(b.size() == 0, "by default contains a size of 0");
  }
  {
    block b(&i, sizeof(i));
    testOk(b.ptr() == &i, "returns the address it has been constructed with");
    testOk(b.size() == sizeof(i), "returns the size it has been constructed with");
  }

  testDiag("A copy constructed memory block object");
  {
    block b1(&i, sizeof(i));
    block b2(b1);
    testOk(b2.ptr() == &i, "returns the address stored in the copied object");
    testOk(b2.size() == sizeof(i), "returns the address stored in the copied object");
  }

  testDiag("Memory block objects pointing to the same block");
  {
    block b1(&i, sizeof(i));
    block b2(&i, sizeof(i));
    testOk(b1 == b2, "are equal");
    testOk(!(b1 != b2), "are not different");
  }
  testDiag("Memory block objects pointing to different blocks");
  {
    block b1(&i, sizeof(i));
    block b2(&j, sizeof(j));
    testOk(!(b1 == b2), "are not equal");
    testOk(b1 != b2, "are different");
  }

  testDiag("Memory block objects");
  {
    block b1(&i, sizeof(i));
    block b2 = b1;
    testOk(b1.ptr() == b2.ptr(), "copy their address when assigned");
    testOk(b1.size() == b2.size(), "copy their size when assigned");
  }
  return testDone();
}
