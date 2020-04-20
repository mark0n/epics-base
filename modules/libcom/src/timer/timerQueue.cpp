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

#define epicsExportSharedSymbols
#include "errlog.h"
#include "timerPrivate.h"

const double timerQueue :: m_exceptMsgMinPeriod = 60.0 * 5.0; // seconds

timerQueue :: timerQueue ( epicsTimerQueueNotify & notifyIn ) :
    Mutex ( __FILE__, __LINE__ ),
    m_exceptMsgTimeStamp ( epicsTime :: getMonotonic ()
                                    - m_exceptMsgMinPeriod ),
    m_notify ( notifyIn ),
    m_pExpTmr ( 0 ),  
    m_processThread ( 0 ), 
    m_numTimers ( 0u ),
    m_cancelPending ( false )
{
}

timerQueue :: ~timerQueue ()
{
    if ( m_heap.size () ) {
        while ( Timer * const pTmr = m_heap.back () ) {    
            pTmr->m_curState = Timer :: stateLimbo;
            m_heap.pop_back ();
        }
    }
}

void timerQueue ::
    m_printExceptMsg ( const char * pName, const type_info & type )
{
    const epicsTime cur = epicsTime :: getMonotonic ();
    const double delay = cur - m_exceptMsgTimeStamp;
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

inline double timerQueue :: m_expDelay ( const epicsTime & currentTime )
{
    double delay = DBL_MAX;
    if ( m_heap.size () > 0u ) {
        delay = m_heap.front ()->m_exp - currentTime;
    }
    return delay;
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
        double delay = m_expDelay ( currentTime );
        if ( delay <= 0.0 ) {
            delay = 0.0;
        }
        return delay;
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
        m_pExpTmr = m_heap.front ();
        epicsTimerNotify * const pTmpNotify = m_pExpTmr->m_pNotify;
        m_pExpTmr->m_pNotify = 0;
        epicsTimerNotify :: expireStatus 
                            expStat ( epicsTimerNotify :: noRestart );
        if ( pTmpNotify ) {
            GuardRelease unguard ( guard );
            debugPrintf ( ( "%5u expired \"%s\" with error %f sec\n", 
                N++, 
                typeid ( *pTmptNotify ).name (), 
                currentTime - m_pExpTmr->m_exp ) );
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
        // !! timer. However, a small amount of overhead is expended
        // !! to properly handle this type of change below.
        //
        if ( m_cancelPending ) {
            //
            // only restart if they didnt cancel() the timer
            // while the call back was running
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
            if ( m_pExpTmr->m_pNotify ) {
                // pNotify was cleared above; if its valid now we 
                // know that someone has started the timer from 
                // another thread and that overrides the restart 
                // parameters from expire
                if ( ! m_fixParent ( m_pExpTmr->m_index ) ) {
                    m_fixChildren ( m_pExpTmr->m_index );
                }
            }
            else if ( expStat.restart() ) {
                // restart as nec
                m_pExpTmr->m_pNotify = pTmpNotify;
                m_pExpTmr->m_exp = currentTime + expStat.expirationDelay ();
                if ( ! m_fixParent ( m_pExpTmr->m_index ) ) {
                    m_fixChildren ( m_pExpTmr->m_index );
                }
            }
            else {
                m_pExpTmr->m_remove ( guard );
            }
        }
        delay = m_expDelay ( currentTime );
    }
    m_pExpTmr = 0; 
    m_processThread = 0;
    return delay;
}

bool timerQueue :: m_fixParent ( size_t childIdx )
{
    bool itMoved = false;
    while ( childIdx != 0u ) {
        const size_t parentIdx = m_parent ( childIdx );
        if ( *m_heap[parentIdx] <= *m_heap[childIdx] ) { 
            break;
        }
        m_swapEntries ( parentIdx, childIdx );
        childIdx = parentIdx;
        itMoved = true;
    }
    return itMoved;
}

void timerQueue :: m_fixChildren ( size_t parentIdx )
{
    const size_t hpsz = m_heap.size ();
    while ( true ) {
        const size_t leftChildIdx = m_leftChild ( parentIdx );  
        const size_t rightChildIdx = m_rightChild ( parentIdx );  
        size_t smallestIdx = parentIdx;
        if ( leftChildIdx < hpsz ) {
            if ( *m_heap[parentIdx] > *m_heap[leftChildIdx] ) {
                smallestIdx = leftChildIdx;
            }
        }
        if ( rightChildIdx < hpsz ) {
            if ( *m_heap[smallestIdx] > *m_heap[rightChildIdx] ) {
                smallestIdx = rightChildIdx;
            }
        }
        if ( smallestIdx == parentIdx ) {
            break;
        }
        m_swapEntries ( parentIdx, smallestIdx );
        parentIdx = smallestIdx;
    } 
}

Timer & timerQueue :: createTimerImpl ()
{
    // better to throw now in contrast with later during start
    Guard guard ( *this );
    m_numTimers++;
    m_heap.reserve ( m_numTimers );
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
                (unsigned long) m_heap.size () );
    if ( level >= 1u ) {
        std :: vector < Timer * > :: const_iterator
                                ppTimer = m_heap.begin ();
        while ( ppTimer != m_heap.end () && *ppTimer ) {   
            (*ppTimer)->show ( level - 1u );
            ++ppTimer;
        }
    }
}

