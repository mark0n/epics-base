
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#include <cstddef>

#define epicsExportSharedSymbols
#include "errlog.h"
#include "epicsThread.h"
#include "epicsAssert.h"
#include "epicsStaticInstanceSketchyCmplr.h"

namespace epics {

epicsShareDef char staticInstanceBusy;

//
// c++ 0x specifies the behavior for concurrent
// access to block scope statics but some compiler
// writers, lacking clear guidance in the earlier
// c++ standards, curiously implement thread unsafe
// block static variables despite ensuring for
// proper multi-threaded behavior for many other
// aspects of the compiler infrastructure such as
// runtime support for exception handling
//
// see also c++ faq, static initialization order fiasco
//
// This implementation is active if we don't
// know for certain that we can trust the compiler.
// This implementation is substantially more efficient 
// at runtime than epicsThreadOnce
//
// we are careful to create T no more than once here
//
EpicsAtomicPtrT staticInstanceInit ( EpicsAtomicPtrT & target,
                            const PStaticInstanceFactory pFactory )
{
    static const std :: size_t spinDownInit = 1000u;
    static const std :: size_t spinCount = 10u;
    STATIC_ASSERT ( spinDownInit > spinCount );
    static const std :: size_t spinThresh = spinDownInit - spinCount;
    std :: size_t spinDown = spinDownInit;
    EpicsAtomicPtrT pCur = 0;
    while ( true ) {
        pCur = epics :: atomic :: compareAndSwap ( target, 
                                                pStaticInstanceInit,
                                                & staticInstanceBusy );
        if ( pCur == pStaticInstanceInit ) {
            try {
                pCur = ( * pFactory ) ();
                epics :: atomic :: set ( target, pCur );
                break;
            }
            catch ( ... ) {
                epics :: atomic :: set ( target,
                                    pStaticInstanceInit );
                throw;
            }
        }
        else if ( pCur != & staticInstanceBusy ) {
            break;
        }
        if ( spinDown <= spinThresh ) {
            epicsThreadSleep ( epicsThreadSleepQuantum () );
        }
        if ( spinDown > 0u ) {
            spinDown--;
        }
        else {
            errlogPrintf ( "staticInstanceInit: waiting for another "
                    "thread to finish creating the static instance\n" );
            spinDown = spinThresh;
        }
    }
    return pCur;
}

} // end of name space epics
