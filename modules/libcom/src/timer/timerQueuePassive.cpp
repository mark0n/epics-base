/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
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

//
// Note, a free list for this class is not currently used because of 
// entanglements between the file scope free list destructor and a
// file scope fdManager destructor which is trying to call a 
// destructor for a passive timer queue which is no longer valid
// in pool.
// 

#include <cstdio>

#define epicsExportSharedSymbols
#include "timerPrivate.h"

epicsTimerQueuePassive & 
    epicsTimerQueuePassive :: create ( 
        epicsTimerQueueNotify &notify )
{
    return * new timerQueuePassive ( notify );
}

timerQueuePassive :: timerQueuePassive ( epicsTimerQueueNotify &notifyIn ) :
    m_queue ( notifyIn ) {}

timerQueuePassive::~timerQueuePassive () {}

epicsTimer & timerQueuePassive::createTimer ()
{
    return m_queue.createTimer ();
}

TimerForC & timerQueuePassive :: createTimerForC ( 
        epicsTimerCallback pCallback, void * pArg )
{
    return m_queue.createTimerForC ( pCallback, pArg );
}

double timerQueuePassive :: process ( 
    const epicsTime & currentTime )
{
    return m_queue.process ( currentTime );
}

void timerQueuePassive :: show ( 
    unsigned int level ) const
{
    printf ( "EPICS non-threaded timer queue at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level >=1u ) {
        m_queue.show ( level - 1u );
    }
}

epicsTimerQueue & timerQueuePassive :: getEpicsTimerQueue () 
{
    return static_cast < epicsTimerQueue &> ( * this );
}

