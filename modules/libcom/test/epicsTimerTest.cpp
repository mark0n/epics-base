/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <atomic>

#include "epicsTimer.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsAssert.h"
#include "tsFreeList.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

using std :: sqrt;
using std :: nothrow;

static epicsEvent expireEvent;
static epicsMutex globalMutex;

class DelayVerify : public epicsTimerNotify {
public:
    DelayVerify ( double expectedDelay, size_t iterations, 
                                        epicsTimerQueue & );
    ~DelayVerify ();
    bool cancel ();
    unsigned start ();
    double delay () const;
    double checkError () const;
    expireStatus expire ( const epicsTime & );
    size_t iterations () const;
    size_t startCount () const;
    size_t expireCount () const;
    static unsigned expirePendingCount;
    static const double m_offset;  
private:
    epicsTimer & m_timer;
    epicsTime m_beginStamp;
    epicsTime m_expireStamp;
    const double m_expectedDelay;
    const size_t m_iterations;
    size_t m_iterationsLeft;
    size_t m_startCount;
    size_t m_expireCount;
    bool m_canceled;
    DelayVerify ( const DelayVerify & );
    DelayVerify & operator = ( const DelayVerify & );
};

const double DelayVerify :: m_offset = 1.0; // sec 
unsigned DelayVerify :: expirePendingCount = 0u;

DelayVerify :: DelayVerify ( double expectedDelayIn, 
                                size_t iterations,
                                epicsTimerQueue & queue ) :
    m_timer ( queue.createTimer() ), 
    m_expectedDelay ( expectedDelayIn ),
    m_iterations ( iterations ),
    m_iterationsLeft ( iterations ),
    m_startCount ( 0u ),
    m_expireCount ( 0u ),
    m_canceled ( false )
{
}

DelayVerify :: ~DelayVerify ()
{
    m_timer.destroy ();
}

inline double DelayVerify :: delay () const
{
    return m_expectedDelay;
}

inline size_t DelayVerify :: iterations () const
{
    return m_iterations;
}

inline size_t DelayVerify :: startCount () const
{
    return m_startCount;
}

inline size_t DelayVerify :: expireCount () const
{
    return m_expireCount;
}

double DelayVerify :: checkError () const
{
    static const double messageThresh = 
        2.0 * epicsThreadSleepQuantum (); // sec 
    if ( m_canceled ) {
        testOk1 ( m_canceled );
        return 0.0;
    }
    double actualDelay =  m_expireStamp - m_beginStamp;
    double errMeasured = actualDelay - m_expectedDelay;
    double errMagnitude = fabs ( errMeasured );
    double errPercent = 
        100.0 * errMagnitude / m_expectedDelay;
    testOk ( errMagnitude < messageThresh,
            "%f.1 ms < %f.1 ms, delay = %f s, (%.1f %%)",
            errMagnitude * 1000.0, 
            messageThresh * 1000.0,
            m_expectedDelay, 
            errPercent );
    return errMeasured;
}

inline bool DelayVerify :: cancel ()
{
    m_canceled = m_timer.cancel ();
    return m_canceled;
}

inline unsigned DelayVerify :: start ()
{
    epicsGuard < epicsMutex > gd ( globalMutex );
    m_beginStamp = epicsTime::getCurrent ();
    m_startCount++;
    return m_timer.start ( *this, m_beginStamp + m_expectedDelay );
}

epicsTimerNotify :: expireStatus 
    DelayVerify :: expire ( const epicsTime & currentTime )
{
    epicsGuard < epicsMutex > gd ( globalMutex );
    m_expireCount++;
    m_expireStamp = currentTime;
    assert ( DelayVerify :: expirePendingCount > 0u );
    if ( --DelayVerify :: expirePendingCount == 0u ) {
        expireEvent.signal ();
    }
    if ( m_iterationsLeft > 1u ) {
        m_iterationsLeft--;
        m_beginStamp = currentTime;
        // test two paths
        // o start called during timer expire callback
        // o return restart requested parameters 
        if ( ( DelayVerify :: expirePendingCount % 2 ) == 0u ) {
            m_timer.start ( *this, currentTime + m_expectedDelay );
            return noRestart;
        }
        else {
            return expireStatus ( restart, m_expectedDelay );
        }
    }
    else {
        return noRestart;
    }
}

static const unsigned nAccuracyTimers = 100000u;

//
// verify reasonable timer interval accuracy
//
void testAccuracy ()
{
    DelayVerify * pTimers[nAccuracyTimers];
    unsigned timerCount = 0;
    unsigned i;

    testDiag ( "Testing timer accuracy N timers = %u", nAccuracyTimers );

    epicsTimerQueueActive & queue = 
        epicsTimerQueueActive::allocate ( true, epicsThreadPriorityMax );

    {
        epicsGuard < epicsMutex > gd ( globalMutex );
        DelayVerify :: expirePendingCount = 0u;
    }

    const size_t nAccuracyTimersD2 = nAccuracyTimers / 2u;
    const size_t nAccuracyTimersD4 = nAccuracyTimers / 4u;
    const size_t nAccuracyTimersD2PD4 = 
                        nAccuracyTimersD2 + nAccuracyTimersD4;
    for ( i = 0u; i < nAccuracyTimers; i++ ) {
        double expectedDelay = i * static_cast < unsigned > ( rand () );
        expectedDelay /= RAND_MAX;
        expectedDelay += DelayVerify :: m_offset;
        size_t nIterations;
        if ( i < nAccuracyTimersD4 ) {
            nIterations = 3;
        }
        else {
            nIterations = 1;
        }
        pTimers[i] = new ( nothrow ) 
                    DelayVerify ( expectedDelay, nIterations, queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nAccuracyTimers );

    size_t totStarted = 0u;
    size_t totExpireExpected = 0u;
    for ( i = 0u; i < nAccuracyTimers; i++ ) {
        const size_t nNewStarted = pTimers[i]->start ();
        const size_t nNewTot = nNewStarted * pTimers[i]->iterations ();
        totStarted += nNewStarted;
        epicsGuard < epicsMutex > gd ( globalMutex );
        DelayVerify :: expirePendingCount += nNewTot;
        totExpireExpected += nNewTot; 
    }
    testOk1 ( totStarted == nAccuracyTimers );

    // restart half of them, to test the accuracy on that path
    for ( i = nAccuracyTimersD2PD4; i < nAccuracyTimers; i++ ) {
        const size_t nNewExpire = pTimers[i]->start ();
        epicsGuard < epicsMutex > gd ( globalMutex );
        DelayVerify :: expirePendingCount += nNewExpire;
        totExpireExpected += nNewExpire; 
    }
    // cancel one quarter of them
    for ( i = nAccuracyTimersD2; i < nAccuracyTimersD2PD4; i++ ) {
        if ( pTimers[i]->cancel () ) {
            epicsGuard < epicsMutex > gd ( globalMutex );
            totExpireExpected--;
            DelayVerify :: expirePendingCount--;
        }
    }
    testDiag ( "Waiting for %u timer callbacks", 
                DelayVerify :: expirePendingCount );

    // wait for all of them to finish
    {
        epicsGuard < epicsMutex > gd ( globalMutex );
        while ( DelayVerify :: expirePendingCount != 0u ) {
            epicsGuardRelease < epicsMutex > gr ( gd );
            expireEvent.wait ();
        }
    }
    double E = 0.0;
    double EE = 0.0;
    for ( i = 0u; i < nAccuracyTimers; i++ ) {
        const double err = pTimers[i]->checkError ();
        E += err;
        EE += err * err; 
    }
    const double mean = E / nAccuracyTimers;
    const double stdDev = sqrt ( EE / nAccuracyTimers - mean * mean );
    testDiag ( "timer delay error mean=%f std dev=%f ms", 
                 mean * 1000.0, stdDev * 1000.0 );
    testOk1 ( fabs ( mean ) < 5e-3 ); // seconds
    testOk1 ( stdDev < 5e-3 ); // seconds
    unsigned nExpire = 0u;
    for ( i = 0u; i < nAccuracyTimers; i++ ) {
        nExpire += pTimers[i]->expireCount ();
        delete pTimers[i];
    }
    testOk1 ( nExpire == totExpireExpected ); 
    queue.release ();
}

class CancelVerify : public epicsTimerNotify {
public:
    CancelVerify ( epicsTimerQueue & );
    ~CancelVerify ();
    void start ( const epicsTime &expireTime );
    void cancel ();
    expireStatus expire ( const epicsTime & );
    static unsigned cancelCount;
    static unsigned expireCount;
private:
    epicsTimer & m_timer;
    CancelVerify ( const CancelVerify & );
    CancelVerify & operator = ( const CancelVerify & );
};

unsigned CancelVerify :: cancelCount;
unsigned CancelVerify :: expireCount;

CancelVerify :: CancelVerify ( epicsTimerQueue &queueIn ) :
    m_timer ( queueIn.createTimer () )
{
}

CancelVerify :: ~CancelVerify ()
{
    m_timer.destroy ();
}

inline void CancelVerify :: start ( const epicsTime &expireTime )
{
    m_timer.start ( *this, expireTime );
}

inline void CancelVerify :: cancel ()
{
    m_timer.cancel ();
    ++CancelVerify::cancelCount;
}

epicsTimerNotify :: expireStatus 
    CancelVerify :: expire ( const epicsTime & )
{
    ++CancelVerify :: expireCount;
    double root = 3.14159;
    for ( unsigned i = 0u; i < 1000; i++ ) {
        root = sqrt ( root );
    }
    return noRestart;
}

//
// verify that expire() won't be called after the timer is cancelled
//
void testCancel ()
{
    static const unsigned nTimers = 25u;
    CancelVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;

    CancelVerify :: cancelCount = 0;
    CancelVerify :: expireCount = 0;

    testDiag ( "Testing timer cancellation" );

    epicsTimerQueueActive &queue =
        epicsTimerQueueActive :: allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new CancelVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );
    if ( ! testOk1 ( CancelVerify :: expireCount == 0 ) )
        testDiag ( "expireCount = %u", CancelVerify :: expireCount );
    if ( ! testOk1 ( CancelVerify :: cancelCount == 0 ) )
        testDiag ( "cancelCount = %u", CancelVerify :: cancelCount );

    testDiag ( "starting %d timers", nTimers );
    epicsTime exp = epicsTime :: getCurrent () + 4.0;
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( exp );
    }
    testOk1 ( CancelVerify :: expireCount == 0 );
    testOk1 ( CancelVerify :: cancelCount == 0 );

    testDiag ( "cancelling timers" );
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->cancel ();
        delete pTimers[i];
    }
    testOk1 ( CancelVerify :: expireCount == 0 );
    testOk1 ( CancelVerify :: cancelCount == nTimers );

    testDiag ( "waiting until after timers should have expired" );
    epicsThreadSleep ( 5.0 );
    testOk1 ( CancelVerify :: expireCount == 0 );
    testOk1 ( CancelVerify :: cancelCount == nTimers );

    epicsThreadSleep ( 1.0 );
    queue.release ();
}

class ExpireDestroyVerify : public epicsTimerNotify {
public:
    ExpireDestroyVerify ( epicsTimerQueue & );
    void start ( const epicsTime &expireTime );
    expireStatus expire ( const epicsTime & );
    static unsigned destroyCount;
protected:
    virtual ~ExpireDestroyVerify ();
private:
    epicsTimer & m_timer;
    ExpireDestroyVerify ( const ExpireDestroyVerify & );
    ExpireDestroyVerify & operator = ( const ExpireDestroyVerify & );
};

unsigned ExpireDestroyVerify :: destroyCount;

ExpireDestroyVerify :: ExpireDestroyVerify ( epicsTimerQueue & queueIn ) :
    m_timer ( queueIn.createTimer () )
{
}

ExpireDestroyVerify :: ~ExpireDestroyVerify ()
{
    m_timer.destroy ();
    ++ExpireDestroyVerify :: destroyCount;
}

inline void ExpireDestroyVerify :: start ( const epicsTime & expireTime )
{
    m_timer.start ( *this, expireTime );
}

epicsTimerNotify :: expireStatus 
        ExpireDestroyVerify :: expire ( const epicsTime & )
{
    delete this;
    return noRestart;
}

//
// verify that a timer can be destroyed in expire
//
void testExpireDestroy ()
{
    static const unsigned nTimers = 25u;
    ExpireDestroyVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;
    ExpireDestroyVerify :: destroyCount = 0;

    testDiag ( "Testing timer destruction in expire()" );

    epicsTimerQueueActive &queue =
        epicsTimerQueueActive :: allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new ExpireDestroyVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );
    testOk1 ( ExpireDestroyVerify :: destroyCount == 0 );

    testDiag ( "starting %d timers", nTimers );
    epicsTime cur = epicsTime :: getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur );
    }

    testDiag ( "waiting until all timers should have expired" );
    epicsThreadSleep ( 5.0 );

    testOk1 ( ExpireDestroyVerify :: destroyCount == nTimers );
    queue.release ();
}


class PeriodicVerify : public epicsTimerNotify {
public:
    PeriodicVerify ( epicsTimerQueue & );
    ~PeriodicVerify ();
    void start ( const epicsTime &expireTime );
    void cancel ();
    expireStatus expire ( const epicsTime & );
    bool verifyCount ();
private:
    epicsTimer & m_timer;
    unsigned m_nExpire;
    bool m_cancelCalled;
    PeriodicVerify ( const PeriodicVerify & );
    PeriodicVerify & operator = ( const PeriodicVerify & );
};

PeriodicVerify :: PeriodicVerify ( epicsTimerQueue & queueIn ) :
    m_timer ( queueIn.createTimer () ), m_nExpire ( 0u ), 
        m_cancelCalled ( false )
{
}

PeriodicVerify :: ~PeriodicVerify ()
{
    m_timer.destroy ();
}

inline void PeriodicVerify :: start ( const epicsTime &expireTime )
{
    m_timer.start ( *this, expireTime );
}

inline void PeriodicVerify :: cancel ()
{
    m_timer.cancel ();
    m_cancelCalled = true;
}

inline bool PeriodicVerify :: verifyCount ()
{
    return ( m_nExpire > 1u );
}

epicsTimerNotify :: expireStatus 
        PeriodicVerify :: expire ( const epicsTime & )
{
    m_nExpire++;
    double root = 3.14159;
    for ( unsigned i = 0u; i < 1000; i++ ) {
        root = sqrt ( root );
    }
    verify ( ! m_cancelCalled );
    double delay = rand ();
    delay = delay / RAND_MAX;
    delay /= 10.0;
    return expireStatus ( restart, delay );
}

//
// verify periodic timers
//
void testPeriodic ()
{
    static const unsigned nTimers = 25u;
    PeriodicVerify *pTimers[nTimers];
    unsigned i;
    unsigned timerCount = 0;

    testDiag ( "Testing periodic timers" );

    epicsTimerQueueActive &queue =
        epicsTimerQueueActive :: allocate ( true, epicsThreadPriorityMin );

    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i] = new PeriodicVerify ( queue );
        timerCount += pTimers[i] ? 1 : 0;
    }
    testOk1 ( timerCount == nTimers );

    testDiag ( "starting %d timers", nTimers );
    epicsTime cur = epicsTime::getCurrent ();
    for ( i = 0u; i < nTimers; i++ ) {
        pTimers[i]->start ( cur );
    }

    testDiag ( "waiting until all timers should have expired" );
    epicsThreadSleep ( 5.0 );

    bool notWorking = false;
    for ( i = 0u; i < nTimers; i++ ) {
        notWorking |= ! pTimers[i]->verifyCount ();
        pTimers[i]->cancel ();
        delete pTimers[i];
    }
    testOk( ! notWorking, "All timers expiring" );
    epicsThreadSleep ( 1.0 );
    queue.release ();
}

class handler : public epicsTimerNotify
{
public:
  handler(const char* nameIn, epicsTimerQueue& queue, std::function<void()> expireFctIn = [] {}) : name(nameIn), timer(queue.createTimer()), expireFct(expireFctIn) {}
  ~handler() { timer.destroy(); }
  void start(double delay) {
    startTime = epicsTime::getMonotonic();
    timer.start(*this, delay);
  }
  void cancel() { timer.cancel(); }
  expireStatus expire(const epicsTime& currentTime) {
    expireTime = epicsTime::getMonotonic();
    expireCount++;
    return expireStatus(noRestart);
  }
  int getExpireCount() const { return expireCount; }
  double getDelay() const { return expireTime - startTime; }
  std::string getName() const { return name; }
private:
  const std::string name;
  epicsTimer &timer;
  int expireCount = 0;
  epicsTime startTime;
  epicsTime expireTime;
  std::function<void()> expireFct;
};

void verify_delta_t(const double delta_t, const std::string & name) {
  if (!testOk(delta_t >= 0, "%s slept at least for the expected time before expiring", name.c_str())) {
    testDiag("delta t = %g", delta_t);
  }

  // The following test frequently fails on Travis CI (probably due to high
  // load on their machines) so we run it but don't consider it for the overall
  // result:
  if (testImpreciseTiming()) {
    testTodoBegin("imprecise");
  }
  if (!testOk(delta_t < 1e-3, "%s expired less than 1 ms after the expected time", name.c_str())) {
    testDiag("delta t = %g", delta_t);
  }
  if (testImpreciseTiming()) {
    testTodoEnd();
  }
}

void testTimerExpires() {
  testDiag("Timer expires");
  epicsTimerQueueActive &queue = epicsTimerQueueActive::allocate(false);
  {
    handler h1("timer1", queue);
    const double arbitrary_time { 0.3 };
    h1.start(arbitrary_time);

    epicsThreadSleep(arbitrary_time + 0.1);

    testOk(h1.getExpireCount() == 1, "timer expired exactly once");
    verify_delta_t(h1.getDelay() - arbitrary_time, h1.getName());
  } // destroy timer
  queue.release();
  epicsThreadSleep(0.1); // FIXME: this line ensures nice ordering of thread starts/shutdown messages in GDB, it can be removed at any time
}

void testMultipleTimersExpire(std::vector<double> && sleepTime) {
  epicsTimerQueueActive &queue = epicsTimerQueueActive::allocate(false);
  {
    std::vector<handler> hv;
    hv.reserve(3);
    for (unsigned i = 0; i < 3; i++) {
      std::string name("timer" + std::to_string(i));
      hv.emplace_back(name.c_str(), queue);
    }
    for (unsigned i = 0; i < hv.size(); i++) {
      hv[i].start(sleepTime[i]);
    }

    epicsThreadSleep(*std::max_element(sleepTime.cbegin(), sleepTime.cend()) + 0.1);

    for (unsigned i = 0; i < hv.size(); i++) {
      testOk(hv[i].getExpireCount() == 1, "timer expired exactly once");
      verify_delta_t(hv[i].getDelay() - sleepTime[i], hv[i].getName());
    }
  } // destroy timers
  queue.release();
  epicsThreadSleep(0.1); // FIXME: this line ensures nice ordering of thread starts/shutdown messages in GDB, it can be removed at any time
}

void testMultipleTimersExpireFirstTimerExpiresFirst() {
  testDiag("Multiple timers expire - first timer expires first");
  testMultipleTimersExpire({ 1.0, 1.2, 1.1 });
}

void testMultipleTimersExpireLastTimerExpiresFirst() {
  testDiag("Multiple timers expire - last timer expires first");
  testMultipleTimersExpire({ 1.1, 1.2, 1.0 });
}

void testTimerReschedule() {
  testDiag("Reschedule timer");
  epicsTimerQueueActive &queue = epicsTimerQueueActive::allocate(false);
  {
    handler h1("timer1", queue);
    double arbitrary_time { 10.0 };
    h1.start(arbitrary_time);
    arbitrary_time = 0.3;
    h1.start(arbitrary_time);

    epicsThreadSleep(arbitrary_time + 0.1);

    testOk(h1.getExpireCount() == 1, "timer expired exactly once");
    verify_delta_t(h1.getDelay() - arbitrary_time, h1.getName());
  } // destroy timer
  queue.release();
  epicsThreadSleep(0.1); // FIXME: this line ensures nice ordering of thread starts/shutdown messages in GDB, it can be removed at any time
}

void testCancelTimer() {
  testDiag("Cancel timer");
  epicsTimerQueueActive &queue = epicsTimerQueueActive::allocate(false);
  {
    handler h1("timer1", queue);
    h1.start(1.0);
    h1.cancel();

    epicsThreadSleep(2.0);

    testOk(h1.getExpireCount() == 0, "timer expired exactly 0 times");
  } // destroy timer
  queue.release();
  epicsThreadSleep(0.1); // FIXME: this line ensures nice ordering of thread starts/shutdown messages in GDB, it can be removed at any time
}

void testCancelTimerWhileExpireIsRunning() {
  testDiag("Cancel timer while expire handler is running");
  epicsTimerQueueActive &queue = epicsTimerQueueActive::allocate(false);
  {
    epicsEvent expireStarted;
    std::atomic_flag endOfExpireReached = ATOMIC_FLAG_INIT;
    auto longRunningFct = [&] {
      expireStarted.trigger();
      epicsThreadSleep(10.0);
      endOfExpireReached.test_and_set();
    };
    handler h1("timer1", queue, longRunningFct );
    h1.start(0.0);
    expireStarted.wait(); // wait for expire() to be called

    h1.cancel(); // cancel while expire() is running
    bool cancelBlocked = endOfExpireReached.test_and_set();

    testOk(h1.getExpireCount() == 1, "timer expired exactly once");
    testOk(cancelBlocked, "cancel() blocks until all expire() calls are finished");
  } // destroy timer
  queue.release();
  epicsThreadSleep(0.1); // FIXME: this line ensures nice ordering of thread starts/shutdown messages in GDB, it can be removed at any time
}

MAIN ( epicsTimerTest )
{
    testPlan(36+nAccuracyTimers);
    testTimerExpires();
    testMultipleTimersExpireFirstTimerExpiresFirst();
    testMultipleTimersExpireLastTimerExpiresFirst();
    testTimerReschedule();
    testCancelTimer();
    testCancelTimerWhileExpireIsRunning();
    testAccuracy ();
    testCancel ();
    testExpireDestroy ();
    testPeriodic ();
    return testDone();
}
