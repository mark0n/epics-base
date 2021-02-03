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

#include <typeinfo>
#include <string>
#include <stdexcept>
#include <cstdio>

#include "timerPrivate.h"
#include "errlog.h"

Timer :: Timer ( timerQueue & queueIn ) :
    m_queue ( queueIn ), 
    m_curState ( stateLimbo ), 
    m_pNotify ( 0 ),
    m_index ( m_invalidIndex ) 
{
}

Timer :: ~Timer ()
{
    M_CancelStatus cs = { false, false };
    {
        Guard guard ( m_queue );
        cs = m_cancelPvt ( guard );
        m_queue.m_numTimers--;
    }
    // we are careful to wake up the timer queue thread after
    // we no longer hold the lock
    if ( cs.reschedule ) {
        m_queue.m_notify.reschedule ();
    }
}

void Timer :: destroy () 
{
    delete this;    
}

unsigned Timer :: start ( epicsTimerNotify & notify, double delaySeconds )
{
    const epicsTime current = epicsTime :: getCurrent ();
    const epicsTime exp = current + delaySeconds;
    const Timer :: M_StartReturn sr = m_privateStart ( notify, exp );
    // we are careful to wake up the timer queue thread after
    // we no longer hold the lock
    if ( sr.resched ) {
        m_queue.m_notify.reschedule ();
    }
    return sr.numNew;
}

unsigned Timer :: start ( epicsTimerNotify & notify, 
                            const epicsTime & expire )
{
    Timer :: M_StartReturn sr = m_privateStart ( notify, expire );
    // we are careful to wake up the timer queue thread after
    // we no longer hold the lock
    if ( sr.resched ) {
        m_queue.m_notify.reschedule ();
    }
    return sr.numNew;
}

Timer :: M_StartReturn 
    Timer :: m_privateStart ( epicsTimerNotify & notify, 
                                    const epicsTime & expire )
{
    Timer :: M_StartReturn sr;
    Guard locker ( m_queue );
    m_pNotify = & notify;
    if ( m_curState == statePending ) {
        const epicsTime oldExp = m_queue.m_heap.front ()->m_exp;
        m_exp = expire;
        if ( ! m_queue.m_fixParent ( m_index ) ) {
            m_queue.m_fixChildren ( m_index );
        }
        if ( m_queue.m_pExpTmr == this ) {
            // new expire time and notify will override 
            // any restart parameters that may be returned 
            // from the timer expire callback
            sr.numNew = 1u;
            sr.resched = false;
        }
        else {
            sr.numNew = 0u;
            sr.resched = ( oldExp > m_queue.m_heap.front ()->m_exp ); 
        }
    }
    else {
        sr.numNew = 1u;
        m_curState = Timer :: statePending;
        m_index = m_queue.m_heap.size ();
        if ( m_index > 0u ) {
            const epicsTime oldExp = m_queue.m_heap.front ()->m_exp;
            m_exp = expire;
            m_queue.m_heap.push_back ( this );
            m_queue.m_fixParent ( m_index );
            sr.resched = ( oldExp > m_queue.m_heap.front ()->m_exp ); 
        }
        else {
            m_exp = expire;
            m_queue.m_heap.push_back ( this );
            sr.resched = true;
        }
    }
    debugPrintf ( ("Start of \"%s\" with delay %f at %p\n",
                    m_pNotify ? 
                        typeid ( *m_pNotify ).name () :
                        typeid ( m_pNotify ).name (),
                    m_exp - epicsTime::getCurrent (),
                    this ) );
    return sr;
}

void Timer :: m_remove ( Guard & guard )
{
    Timer * const pMoved = m_queue.m_heap.back ();
    m_queue.m_heap.pop_back ();
    if ( m_index != m_queue.m_heap.size () ) {
        const size_t oldIndex = m_index;
        m_queue.m_heap[oldIndex] = pMoved;
        pMoved->m_index = oldIndex;
        if ( ! m_queue.m_fixParent ( oldIndex ) ) {
            m_queue.m_fixChildren ( oldIndex );
        }
    }
    m_index = m_invalidIndex;
    m_curState = stateLimbo;
}

bool Timer :: cancel ()
{
    M_CancelStatus cs;
    {
        Guard guard ( m_queue );
        cs = m_cancelPvt ( guard );
    }
    // we are careful to wake up the timer queue thread after
    // we no longer hold the lock
    if ( cs.reschedule ) {
        m_queue.m_notify.reschedule ();
    }
    return cs.wasPending;
}

Timer :: M_CancelStatus Timer :: m_cancelPvt ( Guard & gd )
{
    gd.assertIdenticalMutex ( m_queue );
    M_CancelStatus cs = { false, false };
    Guard guard ( m_queue );
    if ( m_curState == statePending ) {
        m_remove ( guard );
        m_queue.m_cancelPending = ( m_queue.m_pExpTmr == this );
        if ( m_queue.m_cancelPending ) {
            if ( m_queue.m_processThread != epicsThreadGetIdSelf() ) {
                // 1) make certain timer expire callback does not run
                // after this cancel method returns
                // 2) don't require that lock is applied while calling
                // expire callback 
                // 3) assume that timer could be deleted in its 
                // expire callback so we don't touch this after lock
                // is released
                while ( m_queue.m_cancelPending &&
                                    m_queue.m_pExpTmr == this ) {
                    GuardRelease unguard ( guard );
                    m_queue.m_cancelBlockingEvent.wait ();
                }
                // in case other threads are waiting
                m_queue.m_cancelBlockingEvent.signal ();
            }
        }
        else {
            cs.wasPending = true;
        }
    }
    return cs;
}

epicsTimer :: expireInfo Timer :: getExpireInfo () const
{
    // taking a lock here guarantees that users will not
    // see brief intervals when a timer isn't active because
    // it is is canceled when start is called
    Guard locker ( m_queue );
    if ( m_curState == statePending ) {
        return expireInfo ( true, m_exp );
    }
    return expireInfo ( false, epicsTime() );
}

void Timer :: show ( unsigned int level ) const
{
    Guard locker ( m_queue );
    double delay;
    if ( m_curState == statePending ) {
        try {
            delay = m_exp - epicsTime::getCurrent();
        }
        catch ( ... ) {
            delay = -DBL_MAX;
        }
    }
    else {
        delay = -DBL_MAX;
    }
    const char *pStateName;
    if ( m_curState == statePending ) {
        pStateName = "pending";
    }
    else if ( m_curState == stateLimbo ) {
        pStateName = "limbo";
    }
    else {
        pStateName = "corrupt";
    }
    printf ( "Timer, state = %s, index = %lu, delay = %f\n",
            pStateName, (unsigned long ) m_index, delay );
    if ( level >= 1u && m_pNotify ) {
        m_pNotify->show ( level - 1u );
    }
}

