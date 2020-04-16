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

#include <limits.h>

#define epicsExportSharedSymbols
#include "timerPrivate.h"

timerQueueActiveMgr::timerQueueActiveMgr () :
    m_mutex ( __FILE__, __LINE__ )
{
}

timerQueueActiveMgr::~timerQueueActiveMgr ()
{
    Guard locker ( m_mutex );
}
    
epicsTimerQueueActiveForC & timerQueueActiveMgr ::
    allocate ( bool okToShare, unsigned threadPriority )
{
    Guard locker ( m_mutex );
    if ( okToShare ) {
        tsDLIter < epicsTimerQueueActiveForC > iter = m_sharedQueueList.firstIter ();
        while ( iter.valid () ) {
            if ( iter->threadPriority () == threadPriority ) {
                assert ( iter->timerQueueActiveMgrPrivate::referenceCount < UINT_MAX );
                iter->timerQueueActiveMgrPrivate::referenceCount++;
                return *iter;
            }
            iter++;
        }
    }

    epicsTimerQueueActiveForC & queue = 
        * new epicsTimerQueueActiveForC ( okToShare, threadPriority );
    queue.timerQueueActiveMgrPrivate::referenceCount = 1u;
    if ( okToShare ) {
        m_sharedQueueList.add ( queue );
    }
    return queue;
}

void timerQueueActiveMgr ::
    release ( epicsTimerQueueActiveForC & queue )
{
    {
        Guard locker ( m_mutex );
        assert ( queue.timerQueueActiveMgrPrivate::referenceCount > 0u );
        queue.timerQueueActiveMgrPrivate::referenceCount--;
        if ( queue.timerQueueActiveMgrPrivate::referenceCount > 0u ) {
            return;
        }
        else if ( queue.sharingOK () ) {
            m_sharedQueueList.remove ( queue );
        }
    }
    // delete only after we release the guard in case the embedded
    // reference is the last one and this object is destroyed
    // as a side effect
    timerQueueActiveMgrPrivate * const pPriv = & queue;
    delete pPriv;
}

timerQueueActiveMgrPrivate::timerQueueActiveMgrPrivate () :
    referenceCount ( 0u )
{
}

timerQueueActiveMgrPrivate::~timerQueueActiveMgrPrivate () 
{
}
