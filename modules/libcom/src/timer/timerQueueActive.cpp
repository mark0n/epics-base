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

#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsGuard.h"
#include "timerPrivate.h"

epicsTimerQueueActive & epicsTimerQueueActive :: allocate ( bool okToShare, 
                                                    unsigned threadPriority )
{
    return timerQueueActiveMgr :: master ().
        allocate ( okToShare, threadPriority );
}

timerQueueActive ::
    timerQueueActive ( bool okToShareIn, unsigned priority ) :
    m_queue ( *this ), 
    m_thread ( *this, "timerQueue", 
        epicsThreadGetStackSize ( epicsThreadStackMedium ), priority ),
    m_okToShare ( okToShareIn ), 
    m_exitFlag ( false ), 
    m_terminateFlag ( false )
{
    epicsGuard < epicsMutex > guard ( m_queue );
    m_thread.start ();
}

timerQueueActive :: ~timerQueueActive ()
{
    Guard guard ( m_queue );
    m_terminateFlag = true;
    m_rescheduleEvent.signal ();
    while ( ! m_exitFlag ) {
        GuardRelease release ( guard );
        m_exitEvent.wait ( 1.0 );
    }
    // in case other threads are waiting here also
    m_exitEvent.signal ();
}

void timerQueueActive :: run ()
{
    Guard guard ( m_queue );
    m_exitFlag = false;
    while ( ! m_terminateFlag ) {
        double delay = m_queue.process ( guard, epicsTime::getCurrent() );
        {
            GuardRelease release ( guard );
            debugPrintf ( ( "timer thread sleeping for %g sec (max)\n", delay ) );
            m_rescheduleEvent.wait ( delay );
        }
    }
    m_exitFlag = true; 
    m_exitEvent.signal (); // no access to queue after exitEvent signal
}

epicsTimer & timerQueueActive :: createTimer ()
{
    return m_queue.createTimer();
}

TimerForC & timerQueueActive :: createTimerForC ( 
    epicsTimerCallback pCallback, void * pArg )
{
    return m_queue.createTimerForC ( pCallback, pArg );
}

void timerQueueActive :: reschedule ()
{
    m_rescheduleEvent.signal ();
}

void timerQueueActive :: show ( unsigned int level ) const
{
    Guard guard ( const_cast < timerQueue & > ( m_queue ) );
    printf ( "EPICS threaded timer queue at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        // specifying level one here avoids recursive 
        // show callback
        m_thread.show ( 1u );
        m_queue.show ( guard, level - 1u );
        printf ( "reschedule event\n" );
        m_rescheduleEvent.show ( level - 1u );
        printf ( "exit event\n" );
        m_exitEvent.show ( level - 1u );
        printf ( "exitFlag = %c, terminateFlag = %c\n",
                    m_exitFlag ? 'T' : 'F',
                    m_terminateFlag ? 'T' : 'F' );
    }
}

epicsTimerQueue & timerQueueActive :: getEpicsTimerQueue () 
{
    return static_cast < epicsTimerQueue &> ( * this );
}

