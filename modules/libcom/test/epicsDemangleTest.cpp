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

#include <string>
#include <typeinfo>

#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsDemangle.h"
#include "epicsAssert.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using std :: size_t;
using namespace epics;
using namespace atomic;

using std :: string;

class MyTestClass {
public:
    MyTestClass () {}
    void fred ( int ) {}
};

static const unsigned N = 200u;
static size_t iterCount = 0u;
static epicsEvent completion;

void showBasicFunc ( void * )
{
    const string result0 = epicsDemangle ( typeid ( MyTestClass ).name () );
    testDiag ( "Symbol %s demangled %s\n",
                typeid ( MyTestClass ).name (), result0.c_str () );
    const string result1 = epicsDemangleTypeName ( typeid ( MyTestClass ) );
    testDiag ( "Type %s demangled %s\n",
                typeid ( MyTestClass ).name (), result1.c_str() );
    if ( epicsAtomicIncrSizeT ( & iterCount ) == N ) {
        completion.signal ();
    }
}

void threadTest ()
{
    const unsigned int stackSize = 
        epicsThreadGetStackSize ( epicsThreadStackSmall );

    // try to get them all running at once
    // (also check for thread private cleanup problems in valgrind)
    for ( size_t i = 0u; i < N; i++ ) {
        epicsThreadCreate ( "test", 100, stackSize, showBasicFunc, 0 );
    }
    while ( epicsAtomicGetSizeT ( & iterCount ) < N ) {
        completion.wait ();
    }
}


MAIN ( epicsDemangleTest )
{
    testPlan ( 0 );
    threadTest (); 
    return testDone();
}

