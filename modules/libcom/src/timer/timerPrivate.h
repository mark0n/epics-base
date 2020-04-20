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
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef epicsTimerPrivate_h
#define epicsTimerPrivate_h

#include <typeinfo>
#include <vector>

#include "tsDLList.h"
#include "epicsTimer.h"
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "compilerDependencies.h"
#include "epicsStaticInstance.h"
#include "AllocatorArena.h"

#ifdef DEBUG
#   define debugPrintf(ARGSINPAREN) printf ARGSINPAREN
#else
#   define debugPrintf(ARGSINPAREN)
#endif

using std :: type_info;

class Timer;
class timerQueue;

bool operator < ( const Timer &, const Timer & );
bool operator > ( const Timer &, const Timer & );
bool operator <= ( const Timer &, const Timer & );
bool operator >= ( const Timer &, const Timer & );

class Timer : public epicsTimer {
public:
    typedef epicsMutex Mutex;
    typedef epicsGuard < Mutex > Guard;
    typedef epicsGuardRelease < Mutex > GuardRelease;
    Timer ( timerQueue & );
    ~Timer ();
    void destroy ();
    unsigned start ( class epicsTimerNotify &, const epicsTime & );
    unsigned start ( class epicsTimerNotify &, double delaySeconds );
    bool cancel ();
    expireInfo getExpireInfo () const;
    double getExpireDelay ( const epicsTime & currentTime );
    void show ( unsigned int level ) const;
    static void * operator new ( size_t sz );
    static void operator delete ( void * ptr, size_t sz );
protected:
    timerQueue & m_queue;
private:
    typedef epics :: AllocatorArena < Timer, timerQueue, 16u > M_Allocator;
    enum state {
        statePending = 45,
        stateLimbo = 78 };
    epicsTime m_exp; // experation time
    state m_curState; // current state
    epicsTimerNotify * m_pNotify; // callback
    size_t m_index;
    static const size_t m_invalidIndex = 
			~ static_cast < size_t > ( 0u );
    struct M_StartReturn {
        unsigned numNew;
        bool resched;
    };
    M_StartReturn m_privateStart ( epicsTimerNotify & notify, 
	                                const epicsTime & expire );
    struct M_CancelStatus {
        bool reschedule;
        bool wasPending;
    };
    M_CancelStatus m_cancelPvt ( Guard & );
    void m_remove ( Guard & guard );
    Timer & operator = ( const Timer & );
    friend class timerQueue;
    friend bool operator < ( const Timer &, const Timer & );
    friend bool operator > ( const Timer &, const Timer & );
    friend bool operator <= ( const Timer &, const Timer & );
    friend bool operator >= ( const Timer &, const Timer & );
};

struct TimerForC : public epicsTimerNotify {
public:
    typedef epicsMutex Mutex;
    typedef epicsGuard < Mutex > Guard;
    TimerForC ( class timerQueue &, epicsTimerCallback, void * pPrivateIn );
    ~TimerForC ();
    unsigned start ( const epicsTime & );
    unsigned start ( double delaySeconds );
    bool cancel ();
    void show ( unsigned level ) const;
    expireStatus expire ( const epicsTime & currentTime );
    double getExpireDelay ();
    static void * operator new ( size_t sz );
    static void operator delete ( void * ptr, size_t sz );
private:
    typedef epics :: AllocatorArena < TimerForC, timerQueue, 16u > M_Allocator;
    Timer * m_pTimer;
    epicsTimerCallback m_pCallBack;
    void * m_pPrivate;
    TimerForC & operator = ( const TimerForC & );
}; 

class timerQueue : 
    public epicsTimerQueue, 
    public epicsMutex {
public:
    typedef epicsMutex Mutex;
    typedef epicsGuard < Mutex > Guard;
    typedef epicsGuardRelease < Mutex > GuardRelease;
    timerQueue ( epicsTimerQueueNotify &notify );
    virtual ~timerQueue ();
    epicsTimer & createTimer ();
    Timer & createTimerImpl ();
    TimerForC & createTimerForC (
        epicsTimerCallback pCallback, void *pArg );
    double process ( const epicsTime & currentTime );
    double process ( Guard &, const epicsTime & currentTime );
    void show ( unsigned int level ) const;
    void show ( Guard &, unsigned int level ) const;
private:
    epicsEvent m_cancelBlockingEvent;
    std :: vector < Timer * > m_heap;
    epicsTime m_exceptMsgTimeStamp;
    epicsTimerQueueNotify & m_notify;
    Timer * m_pExpTmr;
    epicsThreadId m_processThread;
    size_t m_numTimers;
    bool m_cancelPending;
    static const double m_exceptMsgMinPeriod;
	timerQueue ( const timerQueue & );
    timerQueue & operator = ( const timerQueue & );
    void m_printExceptMsg ( const char * pName, const type_info & type );
    bool m_fixParent ( size_t childIdx );
    void m_fixChildren ( size_t parentIdx );
    void m_swapEntries ( size_t idx0, size_t idx1 );
    double m_expDelay ( const epicsTime & currentTime );
    static size_t m_parent ( size_t childIdx );
    static size_t m_leftChild ( size_t parentIdx );
    static size_t m_rightChild ( size_t parentIdx );
    friend class Timer;
    friend struct TimerForC;
};

class timerQueueActiveMgr;

class timerQueueActiveMgrPrivate {
public:
    timerQueueActiveMgrPrivate ();
protected:
    virtual ~timerQueueActiveMgrPrivate () = 0;
private:
    unsigned referenceCount;
    friend class timerQueueActiveMgr;
};

class timerQueueActive :
    public epicsTimerQueueActive,
    public epicsThreadRunable,
    public epicsTimerQueueNotify,
    public timerQueueActiveMgrPrivate {
public:
    timerQueueActive ( bool okToShare, unsigned priority );
    epicsTimer & createTimer ();
    TimerForC & createTimerForC (
        epicsTimerCallback pCallback, void *pArg );
    void run ();
    void reschedule ();
    epicsTimerQueue & getEpicsTimerQueue ();
    void show ( unsigned int level ) const;
    bool sharingOK () const;
    unsigned threadPriority () const;
protected:
    ~timerQueueActive ();
private:
    typedef epicsMutex Mutex;
    typedef epicsGuard < Mutex > Guard;
    typedef epicsGuardRelease < Mutex > GuardRelease;
    timerQueue m_queue;
    epicsEvent m_rescheduleEvent;
    epicsEvent m_exitEvent;
    epicsThread m_thread;
    bool m_okToShare;
    bool m_exitFlag;
    bool m_terminateFlag;
	timerQueueActive ( const timerQueueActive & );
    timerQueueActive & operator = ( const timerQueueActive & );
};

class timerQueueActiveMgr {
public:
    static timerQueueActiveMgr & master ();
	timerQueueActiveMgr ();
    ~timerQueueActiveMgr ();
    epicsTimerQueueActiveForC & allocate ( bool okToShare,
        unsigned threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueActiveForC & );
private:
    typedef epicsMutex Mutex;
    typedef epicsGuard < Mutex > Guard;
    Mutex m_mutex;
    tsDLList < epicsTimerQueueActiveForC > m_sharedQueueList;
	timerQueueActiveMgr ( const timerQueueActiveMgr & );
    timerQueueActiveMgr & operator = ( const timerQueueActiveMgr & );
};

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer ();
    TimerForC & createTimerForC (
        epicsTimerCallback pCallback, void *pArg );
    void show ( unsigned int level ) const;
    double process ( const epicsTime & currentTime );
    epicsTimerQueue & getEpicsTimerQueue ();
protected:
    timerQueue m_queue;
    ~timerQueuePassive ();
	timerQueuePassive ( const timerQueuePassive & );
    timerQueuePassive & operator = ( const timerQueuePassive & );
};

struct epicsTimerQueuePassiveForC :
    public epicsTimerQueueNotify, 
    public timerQueuePassive {
public:
    epicsTimerQueuePassiveForC (
        epicsTimerQueueNotifyReschedule,
        epicsTimerQueueNotifyQuantum,
        void * pPrivate );
    void destroy ();
    void reschedule ();
protected:
    ~epicsTimerQueuePassiveForC ();
private:
    epicsTimerQueueNotifyReschedule m_pRescheduleCallback;
    epicsTimerQueueNotifyQuantum m_pSleepQuantumCallback;
    void * m_pPrivate;
};

struct epicsTimerQueueActiveForC : public timerQueueActive,
    public tsDLNode < epicsTimerQueueActiveForC > {
public:
    epicsTimerQueueActiveForC ( bool okToShare, unsigned priority );
    void release ();
protected:
    virtual ~epicsTimerQueueActiveForC ();
private:
	epicsTimerQueueActiveForC ( const epicsTimerQueueActiveForC & );
    epicsTimerQueueActiveForC & operator = ( const epicsTimerQueueActiveForC & );
};

inline double Timer :: getExpireDelay ( const epicsTime & currentTime )
{
	return m_exp - currentTime;
}

inline void * Timer :: operator new  ( size_t sz )
{
    return M_Allocator :: allocateOctets ( sz );
}

inline void Timer :: operator delete ( void * p, size_t sz )
{
    M_Allocator :: deallocateOctets ( p,  sz );
}

inline bool operator < ( const Timer & lhs, const Timer & rhs )
{ 
    return lhs.m_exp < rhs.m_exp;
}

inline bool operator > ( const Timer & lhs, const Timer & rhs )
{ 
    return rhs < lhs; 
}

inline bool operator <= ( const Timer & lhs, const Timer & rhs )
{ 
    return ! ( lhs > rhs ); 
}

inline bool operator >= ( const Timer & lhs, const Timer & rhs )
{ 
    return ! ( lhs < rhs ); 
}

inline size_t timerQueue :: m_parent ( const size_t childIdx )
{
    return ( childIdx + ( childIdx & 1u ) ) / 2u - 1u;
}

inline size_t timerQueue :: m_leftChild ( const size_t parentIdx )
{
    return ( parentIdx + 1u ) * 2u - 1u;
}

inline size_t timerQueue :: m_rightChild ( const size_t parentIdx )
{
    return ( parentIdx + 1u ) * 2u;
}

inline void timerQueue :: m_swapEntries ( size_t idx0, size_t idx1 )
{
    std :: swap ( m_heap[idx0], m_heap[idx1] );
    m_heap[idx0]->m_index = idx0;
    m_heap[idx1]->m_index = idx1;
}

inline bool timerQueueActive :: sharingOK () const
{
    return m_okToShare;
}

inline unsigned timerQueueActive :: threadPriority () const
{
    return m_thread.getPriority ();
}

inline unsigned TimerForC :: start ( const epicsTime & expTime )
{
    return m_pTimer->start ( *this, expTime );
}

inline unsigned TimerForC :: start ( double delaySeconds )
{
    return m_pTimer->start ( *this, delaySeconds );
}

inline bool TimerForC :: cancel ()
{
    return m_pTimer->cancel ();
}

inline double TimerForC :: getExpireDelay ()
{
    return m_pTimer->getExpireDelay ( epicsTime :: getCurrent () );
}

inline void * TimerForC :: operator new  ( size_t sz )
{
    return M_Allocator :: allocateOctets ( sz );
}

inline void TimerForC :: operator delete ( void * p, size_t sz )
{
    M_Allocator :: deallocateOctets ( p,  sz );
}

inline timerQueueActiveMgr & timerQueueActiveMgr :: master ()
{
    return epics :: staticInstance < timerQueueActiveMgr > ();
}

#endif // epicsTimerPrivate_h

