/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
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
#include <string>
#include <stdexcept>

#include "epicsMath.h"
#include "epicsTimer.h"
#include "timerPrivate.h"

epicsTimer::~epicsTimer () {}

void epicsTimerNotify :: show ( unsigned /* level */ ) const {}

TimerForC :: TimerForC ( timerQueue & queue, epicsTimerCallback const pCB, 
                                                    void * const pPrivate ) :
    m_pTimer ( & queue.createTimerImpl () ), 
    m_pCallBack ( pCB ), 
    m_pPrivate ( pPrivate )
{
}

TimerForC :: ~TimerForC ()
{
    m_pTimer->destroy ();
}

void TimerForC :: show ( unsigned level ) const
{
    printf ( "TimerForC: callback ptr %p private ptr %p\n", 
                m_pCallBack, m_pPrivate );
    if ( level > 1 && m_pTimer ) {
        m_pTimer->show ( level - 1 );
    }
}

epicsTimerNotify :: expireStatus 
    TimerForC :: expire ( const epicsTime & )
{
    ( *m_pCallBack ) ( m_pPrivate );
    return noRestart;
}

epicsTimerQueueActiveForC ::
        epicsTimerQueueActiveForC ( bool okToShare, unsigned priority ) :
    timerQueueActive ( okToShare, priority )
{
}

epicsTimerQueueActiveForC :: ~epicsTimerQueueActiveForC ()
{
}

void epicsTimerQueueActiveForC :: release ()
{
    timerQueueActiveMgr :: master ().release ( *this );
}

epicsTimerQueuePassiveForC :: epicsTimerQueuePassiveForC ( 
        epicsTimerQueueNotifyReschedule pRescheduleCallback, 
        epicsTimerQueueNotifyQuantum pSleepQuantumCallback,
        void * pPrivate ) :
    timerQueuePassive ( 
        * static_cast < epicsTimerQueueNotify * > ( this ) ), 
    m_pRescheduleCallback ( pRescheduleCallback ), 
    m_pSleepQuantumCallback ( pSleepQuantumCallback ),
    m_pPrivate ( pPrivate )
{
}

epicsTimerQueuePassiveForC :: ~epicsTimerQueuePassiveForC () 
{
}

void epicsTimerQueuePassiveForC :: reschedule ()
{
    ( *m_pRescheduleCallback ) ( m_pPrivate );
}

void epicsTimerQueuePassiveForC :: destroy ()
{
    delete this;
}

epicsShareFunc epicsTimerNotify :: expireStatus :: 
                            expireStatus ( restart_t restart ) : 
    m_delay ( -DBL_MAX )
{
    if ( restart != noRestart ) {
        throw std::logic_error
            ( "timer restart was requested "
                "without specifying a delay?" );
    }
}

epicsShareFunc epicsTimerNotify :: expireStatus :: expireStatus 
            ( restart_t restartIn, const double & expireDelaySec ) :
    m_delay ( expireDelaySec )
{
    if ( restartIn != epicsTimerNotify :: restart ) {
        throw std::logic_error
            ( "no timer restart was requested, "
                "but a delay was specified?" );
    }
    if ( m_delay < 0.0 || ! finite ( m_delay ) ) {
        throw std::logic_error
            ( "timer restart was requested, but a "
                "negative delay was specified?" );
    }
}

epicsShareFunc bool 
        epicsTimerNotify :: expireStatus :: restart () const
{
    return m_delay >= 0.0 && finite ( m_delay );
}

epicsShareFunc double 
        epicsTimerNotify :: expireStatus :: expirationDelay () const
{
    if ( m_delay < 0.0 || ! finite ( m_delay ) ) {
        throw std::logic_error
            ( "no timer restart was requested, "
                "but you are asking for a restart delay?" );
    }
    return m_delay;
}

extern "C" epicsTimerQueuePassiveId epicsStdCall
    epicsTimerQueuePassiveCreate (
        const epicsTimerQueueNotifyReschedule pRescheduleCallback, 
        const epicsTimerQueueNotifyQuantum pSleepQuantumCallback,
        void * const pPrivate )
{
    try {
        return new epicsTimerQueuePassiveForC (
                                    pRescheduleCallback, 
                                    pSleepQuantumCallback,
                                    pPrivate );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsStdCall 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId pQueue )
{
    pQueue->destroy ();
}

extern "C" double epicsStdCall 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId pQueue )
{
    try {
        return pQueue->process ( epicsTime::getCurrent() );
    }
    catch ( ... ) {
        return 1.0;
    }
}

extern "C" epicsTimerId epicsShareAPI 
        epicsTimerQueuePassiveCreateTimer ( 
                            epicsTimerQueuePassiveId pQueue, 
                            epicsTimerCallback pCallback, void *pArg )
{
    try {
        return & pQueue->createTimerForC ( pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" epicsShareFunc void epicsShareAPI 
    epicsTimerQueuePassiveDestroyTimer (   
        epicsTimerQueuePassiveId pQueue, epicsTimerId pTmr )
{
    delete pTmr;
}

extern "C" void  epicsStdCall epicsTimerQueuePassiveShow (
    epicsTimerQueuePassiveId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" epicsTimerQueueId epicsStdCall
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsTimerQueueActiveForC & tmr =
            timerQueueActiveMgr :: master ().
                allocate ( okToShare ? true : false, threadPriority );
        return & tmr;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsStdCall epicsTimerQueueRelease ( epicsTimerQueueId pQueue )
{
    pQueue->release ();
}

extern "C" epicsTimerId epicsShareAPI 
    epicsTimerQueueCreateTimer (
        epicsTimerQueueId pQueue, 
        epicsTimerCallback pCallback, void *pArg )
{
    try {
        return & pQueue->createTimerForC ( pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsShareAPI 
    epicsTimerQueueShow (
        epicsTimerQueueId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" void epicsShareAPI 
    epicsTimerQueueDestroyTimer ( 
        epicsTimerQueueId pQueue, epicsTimerId pTmr )
{
    delete pTmr;
}

extern "C" unsigned epicsShareAPI 
    epicsTimerStartTime ( 
        epicsTimerId pTmr, const epicsTimeStamp *pTime )
{
    return pTmr->start ( *pTime );
}

extern "C" unsigned epicsShareAPI 
    epicsTimerStartDelay ( 
        epicsTimerId pTmr, double delaySeconds )
{
    return pTmr->start ( delaySeconds );
}

extern "C" int epicsShareAPI 
    epicsTimerCancel ( epicsTimerId pTmr )
{
    return pTmr->cancel ();
}

extern "C" double  epicsShareAPI 
    epicsTimerGetExpireDelay ( epicsTimerId pTmr )
{
    return pTmr->getExpireDelay ();
}

extern "C" void  epicsShareAPI 
    epicsTimerShow ( epicsTimerId pTmr, unsigned int level )
{
    pTmr->show ( level );
}

