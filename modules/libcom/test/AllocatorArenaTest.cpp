
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <cstdlib>

#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "AllocatorArena.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using std :: size_t;
using namespace epics;
using namespace atomic;

static const unsigned N = 200u;
// O chosen to not be even multiple of M
static const unsigned M = 7u;
static const unsigned O = 13u;
static size_t iterCount = 0u;
static epicsEvent completion;

struct TestObj {
    int i;
    float f;
    static void * operator new ( size_t sz );
    static void operator delete ( void * ptr, size_t sz );
    typedef AllocatorArena < TestObj, TestObj, O > Allocator;
};

inline void * TestObj :: operator new  ( size_t sz )
{
    return Allocator :: allocateOctets ( sz );
}

inline void TestObj :: operator delete ( void * p, size_t sz )
{
    Allocator :: deallocateOctets ( p,  sz );
}

static TestObj :: Allocator aa;

static void test ( void *arg )
{
    double delay = rand ();
    delay /= RAND_MAX;
    for ( size_t i = 0u; i < M; i++ ) {
        epicsThreadSleep ( delay );
        TestObj * const p = new TestObj;
        epicsThreadSleep ( delay );
        delete p;
    }
    if ( epicsAtomicIncrSizeT ( & iterCount ) == N ) {
        completion.signal ();
    }
}

MAIN ( epicsAllocatorArenaTest )
{
    TestObj :: Allocator aa;


    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    testPlan(2);

    // try to get them all running at once
    for ( size_t i = 0u; i < N; i++ ) {
        epicsThreadCreate ( "test", 100, stackSize, test, 0 );
    }
    while ( epicsAtomicGetSizeT ( & iterCount ) < N ) {
        completion.wait ();
    }
    testOk ( aa.rackCount () == 0u,
                "all thread private racks deallocated %lu", 
                static_cast < unsigned long > ( aa.rackCount () ) );
    testOk ( aa.byteCount () == 0u,
                "all thread private bytes deallocated %lu", 
                static_cast < unsigned long > ( aa.byteCount () ) );

    return testDone();
}
