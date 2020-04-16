/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
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
#include "epicsStaticInstance.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using namespace epics;
using namespace atomic;
using std :: size_t;

static size_t initCount = 0u;
static size_t iterCount = 0u;

struct Tester {
    Tester () { epicsAtomicIncrSizeT ( & initCount ); }
    void iterate () { epicsAtomicIncrSizeT ( & iterCount ); }
};

static void test ( void *arg )
{
    staticInstance < epicsEvent > ().wait ();
    staticInstance < Tester > ().iterate ();
    staticInstance < epicsEvent > ().signal ();
}

MAIN ( epicsStaticInstanceTest )
{
    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    testPlan(2);

    static const unsigned N = 100;
    for ( size_t i = 0u; i < N; i++ ) {
        epicsThreadCreate ( "test", 100, stackSize, test, 0 );
    }
    // try to get them all running at once
    for ( size_t i = 0u; i < N; i++ ) {
        staticInstance < epicsEvent > ().signal ();
    }
    while ( epicsAtomicGetSizeT ( & iterCount ) < N ) {
        epicsThreadSleep ( 0.01 );
    }
    testOk ( get ( iterCount ) == N,
                "correct test iterations %lu", 
                static_cast < unsigned long > ( iterCount ) );
    testOk ( get ( initCount ) == 1u,
                "only one object was created" );

    return testDone();
}
