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
/* epicsTimer.h */

/* Authors: Marty Kraimer, Jeff Hill */

#ifndef epicsTimerH
#define epicsTimerH

#include "epicsTime.h"
#include "epicsThread.h"
#include "shareLib.h"

#ifdef __cplusplus

#include <cfloat>

/*
 * Notes:
 * 1) The timer queue process method does not hold the timer 
 * queue lock when calling callbacks, to avoid deadlocks.
 *
 * 2) The timer start method has three different possible outcomes
 *
 * 2a) If start is called and the timer isnt pending in the timer
 * queue, and the timer callback isnt currently being orchestrated,
 * then the timer is schedualed in the queue and start returns 
 * 1u, indicating that the timer callback will run once as a 
 * direct consequence of this invocation of start.
 *
 * 2b) If start is called and the timer is already pending in 
 * the timer queue, then the timer is reschedualed back into
 * a new positon in the queue and start returns 0u. The 
 * timer callback will run once as a reschedualed (postponed) 
 * consequence of a previous invocation of start, but zero 
 * times as a direct consequence of this call to start.
 *
 * 2c) If start is called and the timer isnt pending in the timer 
 * queue, but the timer callback _is_ currently being orchestrated
 * then it is reschedualed in the queue for a new expiration 
 * time, and start returns 1u. The timer callback will run twice.
 * Once as a consequence of this invocation of start, and once 
 * as a consequence of a previous call to start.
 *
 * 3) Cancel returns true if the timer was pending in the queue
 * when cancel was called, and false otherwise.
 */

/* code using a timer must implement epicsTimerNotify */
class LIBCOM_API epicsTimerNotify {
public:
    enum restart_t { noRestart, restart };
    class epicsShareClass expireStatus {
    public:
        expireStatus ( restart_t );
        expireStatus ( restart_t, const double & expireDelaySec );
        bool restart () const;
        double expirationDelay () const;
    private:
        double m_delay;
    };

    /* return "noRestart" or "expireStatus ( restart, 30.0 )" */
    virtual expireStatus expire ( const epicsTime & currentTime ) = 0;
    virtual void show ( unsigned int level ) const;
protected:
    virtual ~epicsTimerNotify () {} // protected disables delete through intf
};

class LIBCOM_API epicsTimer {
public:
    /* calls cancel (see warning below) and then destroys the timer */
    virtual void destroy () = 0;
    /* see note 2 above for the significance of the return value */
    virtual unsigned start ( epicsTimerNotify &, const epicsTime & ) = 0;
    virtual unsigned start ( epicsTimerNotify &, double delaySeconds ) = 0;
    /* see note 3 for the significance of the return value
     *
     * WARNING: A deadlock will occur if you hold a lock while
     * calling this function that you also take within the timer
     * expiration callback.
     */
    virtual bool cancel () = 0;
    struct expireInfo {
        expireInfo ( bool active, const epicsTime & expireTime );
        bool active;
        epicsTime expireTime;
    };
    virtual expireInfo getExpireInfo () const = 0;
    double getExpireDelay ();
    virtual void show ( unsigned int level ) const = 0;
protected:
    virtual ~epicsTimer () = 0; /* disable delete */
};

class epicsTimerQueue {
public:
    virtual epicsTimer & createTimer () = 0;
    virtual void show ( unsigned int level ) const = 0;
protected:
    virtual ~epicsTimerQueue () {} /* disable delete */
};

class epicsTimerQueueActive
    : public epicsTimerQueue {
public:
    static LIBCOM_API epicsTimerQueueActive & allocate (
        bool okToShare, unsigned threadPriority = epicsThreadPriorityMin + 10 );
    virtual void release () = 0;
protected:
    virtual ~epicsTimerQueueActive () {} /* disable delete */
};

class epicsTimerQueueNotify {
public:
    /* called when a new timer is inserted into the queue and the */
    /* delay to the next expire has changed */
    virtual void reschedule () = 0;
protected:
    virtual ~epicsTimerQueueNotify () {} /* disable delete */
};

class epicsTimerQueuePassive : public epicsTimerQueue {
public:
    static LIBCOM_API epicsTimerQueuePassive & create ( epicsTimerQueueNotify & );
    virtual ~epicsTimerQueuePassive () {} /* ok to call delete */
    virtual double process ( const epicsTime & currentTime ) = 0; /* returns delay to next expire */
};

inline epicsTimer::expireInfo::expireInfo ( bool activeIn,
    const epicsTime & expireTimeIn ) :
        active ( activeIn ), expireTime ( expireTimeIn )
{
}

inline double epicsTimer :: getExpireDelay ()
{
    epicsTimer::expireInfo info = this->getExpireInfo ();
    if ( info.active ) {
        double delay = info.expireTime - epicsTime::getCurrent ();
        if ( delay < 0.0 ) {
            delay = 0.0;
        }
        return delay;
    }
    return - DBL_MAX;
}

extern "C" {
#endif /* __cplusplus */

typedef struct TimerForC * epicsTimerId;
typedef void ( *epicsTimerCallback ) ( void *pPrivate );

/* thread managed timer queue */
typedef struct epicsTimerQueueActiveForC * epicsTimerQueueId;
LIBCOM_API epicsTimerQueueId epicsStdCall
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority );
LIBCOM_API void epicsStdCall 
    epicsTimerQueueRelease ( epicsTimerQueueId );
LIBCOM_API epicsTimerId epicsStdCall 
    epicsTimerQueueCreateTimer ( epicsTimerQueueId queueid,
        epicsTimerCallback callback, void *arg );
LIBCOM_API void epicsStdCall 
    epicsTimerQueueDestroyTimer ( epicsTimerQueueId queueid, epicsTimerId id );
LIBCOM_API void  epicsStdCall 
    epicsTimerQueueShow ( epicsTimerQueueId id, unsigned int level );

/* passive timer queue */
typedef struct epicsTimerQueuePassiveForC * epicsTimerQueuePassiveId;
typedef void ( * epicsTimerQueueNotifyReschedule ) ( void * pPrivate );
typedef double ( * epicsTimerQueueNotifyQuantum ) ( void * pPrivate );
LIBCOM_API epicsTimerQueuePassiveId epicsStdCall
    epicsTimerQueuePassiveCreate ( epicsTimerQueueNotifyReschedule,
        epicsTimerQueueNotifyQuantum, void *pPrivate );
LIBCOM_API void epicsStdCall 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId );
LIBCOM_API epicsTimerId epicsStdCall 
    epicsTimerQueuePassiveCreateTimer (
        epicsTimerQueuePassiveId queueid, epicsTimerCallback pCallback, void *pArg );
LIBCOM_API void epicsStdCall 
    epicsTimerQueuePassiveDestroyTimer ( epicsTimerQueuePassiveId queueid, epicsTimerId id );
LIBCOM_API double epicsStdCall 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId );
LIBCOM_API void  epicsStdCall 
    epicsTimerQueuePassiveShow ( epicsTimerQueuePassiveId id, unsigned int level );

/* timer */
epicsShareFunc unsigned epicsShareAPI 
    epicsTimerStartTime ( epicsTimerId id, const epicsTimeStamp *pTime );
epicsShareFunc unsigned epicsShareAPI 
    epicsTimerStartDelay ( epicsTimerId id, double delaySeconds );
epicsShareFunc int epicsShareAPI 
    epicsTimerCancel ( epicsTimerId id );
LIBCOM_API double epicsStdCall 
    epicsTimerGetExpireDelay ( epicsTimerId id );
LIBCOM_API void  epicsStdCall 
    epicsTimerShow ( epicsTimerId id, unsigned int level );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* epicsTimerH */
