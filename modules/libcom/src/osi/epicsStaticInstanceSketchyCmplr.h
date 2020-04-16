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

#ifndef epicsStaticInstanceSketchyCmplr_h
#define epicsStaticInstanceSketchyCmplr_h

#include "epicsAtomic.h"

#include "shareLib.h"

namespace epics {

typedef EpicsAtomicPtrT ( * PStaticInstanceFactory ) ();

epicsShareFunc EpicsAtomicPtrT staticInstanceInit ( EpicsAtomicPtrT & target,
                                    const PStaticInstanceFactory pFactory );
                                    
epicsShareExtern char staticInstanceBusy;

namespace {
    template < typename T >
    EpicsAtomicPtrT staticInstanceFactory ()
    {
        return new T ();
    }
    static const EpicsAtomicPtrT pStaticInstanceInit = 0;
}

// this should _not_ be an in line function so that
// we guarantee that only one instance of type T per
// executable is created
template < typename T > T & staticInstance ()
{
    static EpicsAtomicPtrT pInstance = pStaticInstanceInit;
    EpicsAtomicPtrT pCur = epics :: atomic :: get ( pInstance );
    if ( pCur == pStaticInstanceInit || pCur == & staticInstanceBusy ) {
        pCur = staticInstanceInit ( pInstance, staticInstanceFactory < T > );
    }
    return * reinterpret_cast < T * > ( pCur );
}

} // end of name space epics

#endif // ifndef epicsStaticInstanceSketchyCmplr_h
