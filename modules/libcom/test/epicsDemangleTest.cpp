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

#include "epicsDemangle.h"
#include "epicsAssert.h"
#include "epicsUnitTest.h"
#include "testMain.h"


using std :: string;

class MyTestClass {
public:
    MyTestClass () {}
    void fred ( int ) {}
};

void showBasicFunc ()
{
    const string result0 = epicsDemangle ( typeid ( MyTestClass ).name () );
    testDiag ( "Symbol %s demangled %s\n",
                typeid ( MyTestClass ).name (), result0.c_str () );
    const string result1 = epicsDemangleTypeName ( typeid ( MyTestClass ) );
    testDiag ( "Type %s demangled %s\n",
                typeid ( MyTestClass ).name (), result1.c_str() );
}

MAIN ( epicsDemangleTest )
{
    testPlan ( 0 );
    showBasicFunc (); 
    return testDone();
}

