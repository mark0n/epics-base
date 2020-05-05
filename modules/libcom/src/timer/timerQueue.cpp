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

#include <cstdio>
#include <algorithm>

#include "errlog.h"
#include "timerPrivate.h"

const double timerQueue :: m_exceptMsgMinPeriod = 60.0 * 5.0; // seconds

timerQueue :: timerQueue ( epicsTimerQueueNotify & notifyIn ) :
    Mutex ( __FILE__, __LINE__ ),
    m_exceptMsgTimeStamp ( epicsTime :: getCurrent () 
                                    - m_exceptMsgMinPeriod ),
    m_notify ( notifyIn ), 
    m_processThread ( 0 ), 
//     m_numTimers ( 0u ),
    m_cancelPending ( false )
{
}

timerQueue :: ~timerQueue ()
{
    if ( !m_pq.empty() ) {
        for (auto & he : m_pq) {
            he.m_tmr->m_curState = Timer::stateLimbo;
        }
    }
}

void timerQueue ::
    m_printExceptMsg ( const char * pName, const type_info & type )
{
    epicsTime cur = epicsTime :: getCurrent ();
    double delay = cur - m_exceptMsgTimeStamp;
    if ( delay >= m_exceptMsgMinPeriod ) {
        m_exceptMsgTimeStamp = cur;
        char date[64];
        cur.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f" );
        // we dont touch the typeid for the timer expiration
        // notify interface here because they might have
        // destroyed the timer during its callback
        errlogPrintf (
            "timerQueue: Unexpected C++ exception \"%s\" "
            "with type \"%s\" during timer expiration "
            "callback at %s\n",
            pName,
            type.name (),
            date );
        errlogPrintf ( "!!!! WARNING - PERIODIC TIMER MAY NOT RESTART !!!!\n" );
        errlogFlush ();
    }
}

// Return the amount of time we need to sleep until the next timer in the queue
// expires.
inline double timerQueue :: m_expDelay ( const epicsTime & currentTime )
{
    if ( m_pq.empty() )
        return DBL_MAX;

    return m_pq.top().m_tmr->m_exp - currentTime;
}

double timerQueue :: process ( const epicsTime & currentTime )
{
    Guard guard ( *this );
    return this->process ( guard, currentTime );
}

double timerQueue :: process ( Guard & guard,
                                const epicsTime & currentTime )
{  
    guard.assertIdenticalMutex ( *this );
    if ( m_processThread ) {
        // if some other thread is processing the queue
        // (or if this is a recursive call)
        return std::max(0.0, m_expDelay(currentTime));
    }

#   ifdef DEBUG
        unsigned N = 0u;
#   endif

    m_processThread = epicsThreadGetIdSelf ();
    double delay = m_expDelay ( currentTime );
    while ( delay <= 0.0 ) {
        //
        // if delay is zero or less we know at least one timer is on 
        // the queue
        //
        // tag current expired tmr so that we can detect if call back
        // is in progress when canceling the timer
        //
        delay = 0.0;
        epicsTimerNotify * const pTmpNotify = m_pq.top().m_tmr->m_pNotify;
        m_pq.top().m_tmr->m_pNotify = 0;
        epicsTimerNotify :: expireStatus 
                            expStat ( epicsTimerNotify :: noRestart );
        if ( pTmpNotify ) {
            GuardRelease unguard ( guard );
            debugPrintf ( ( "%5u expired \"%s\" with error %f sec\n",
                N++, 
                typeid ( *pTmpNotify ).name (), 
                currentTime - m_pq.top().m_tmr->m_exp ) );
            try {
                expStat = pTmpNotify->expire ( currentTime );
            }
            catch ( std::exception & except ) {
                m_printExceptMsg ( except.what (), typeid ( except ) );
            }
            catch ( ... ) {
                m_printExceptMsg ( "non-standard exception", typeid ( void ) );
            }
        }

        //
        // !! The position of a timer in the queue is allowed to change 
        // !! while its timer callback is running. This happens 
        // !! potentially when they reschedule a timer or cancel a
        // !! timer. A small amount of additional labor is expended
        // !! to properly handle this type of change below.
        //
        if ( m_cancelPending ) {
            //
            // only restart if they didnt cancel() the currently
            // expiring timer while its call-back is running
            //
            // 1) if another thread is canceling then cancel() waits for
            // the event below
            // 2) if this thread is canceling in the timer callback then
            // dont touch timer or notify here because the cancel might
            // have occurred because they destroyed the timer in the
            // callback
            // 3) timer::cancel sets timer state to limbo and timer index
            // to invalid
            //
            m_cancelPending = false;
            m_cancelBlockingEvent.signal ();
        }
        else {
            if ( m_pq.top().m_tmr->m_pNotify ) {
                // pNotify was cleared above; if its valid now we 
                // know that someone has restarted the timer when
                // its callback is currently running either 
                // asynchronously from another thread or from 
                // within the currently running expire callback,
                // possibly moving its position in the heap. As 
                // a defined policy either of these situations
                // overrides any restart request parameters 
                // returned from expire
            }
            else if ( expStat.restart() ) {
                // restart as nec
                m_pq.top().m_tmr->m_pNotify = pTmpNotify;
                m_pq.top().m_tmr->m_exp = currentTime + expStat.expirationDelay ();
                m_pq.update(m_pq.top().m_tmr->m_handle); //FIXME: try to optimize by calling increase()/decrease()
            }
            else {
                m_pq.top().m_tmr->m_remove ( guard );
            }
        }
        delay = m_expDelay ( currentTime );
    }
    m_processThread = 0;
    return delay;
}

Timer & timerQueue :: createTimerImpl ()
{
    // better to throw now in contrast with later during start
    Guard guard ( *this );
//     m_numTimers++;
//     m_pq.reserve(m_numTimers); //FIXME
    return * new Timer ( * this );
}

epicsTimer & timerQueue :: createTimer ()
{
    return createTimerImpl ();
}

TimerForC & timerQueue :: createTimerForC ( 
    epicsTimerCallback pCallback, void *pArg )
{
    return * new TimerForC ( *this, pCallback, pArg );
}

void timerQueue :: show ( unsigned level ) const
{
    Guard guard ( const_cast < timerQueue & > ( *this ) );
    this->show ( guard, level );
}

void timerQueue :: show ( Guard & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( *this );
    printf ( "epicsTimerQueue with %lu items pending\n", 
                (unsigned long) m_pq.size () );
    if ( level >= 1u ) {
        for (auto const & he : m_pq) {
            he.m_tmr->show(level - 1u);
        }
    }
}

