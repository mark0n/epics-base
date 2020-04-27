
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeffrey O. Hill
 */
#include <exception>
#include <cstdlib>

#include <cxxabi.h>

#include "epicsThread.h"
#include "epicsAtomic.h"
#include "epicsExit.h"
#include "epicsAssert.h"

#include "epicsDemangle.h"

using std :: string;

union ThreadPrivateId {
    ThreadPrivateId () : m_epics ( 0 ) {}
    epicsThreadPrivateId m_epics;
    void * m_cmpAndSwap;
};

class ThreadPrivate {
public:
    ThreadPrivate ();
    ~ThreadPrivate ();
    string demangle ( const char * pMangledName );
    static ThreadPrivate * get ();
    static void epicsExit ( void * /* arg */ );
private:
    char * m_pBuf;
    size_t m_bufSize;
    static const size_t m_bufSizeInit;
    static ThreadPrivateId m_privateId;
};

const size_t ThreadPrivate :: m_bufSizeInit = 256u;
ThreadPrivateId ThreadPrivate :: m_privateId;

//
// __cxa_demangle doc indicates that buffer passed must be allocated 
// with malloc
//
ThreadPrivate :: ThreadPrivate () :
    m_pBuf ( static_cast < char * > ( malloc ( m_bufSizeInit ) ) ),
    m_bufSize ( m_pBuf ? m_bufSizeInit : 0u )
{
}

ThreadPrivate :: ~ThreadPrivate ()
{
    //
    // __cxa_demangle doc indicates that buffer passed must be allocated 
    // with malloc
    //
    free ( m_pBuf );
}

string ThreadPrivate :: demangle ( const char * const pMangledName )
{
    if ( ! m_pBuf ) {
        //
        // __cxa_demangle doc indicates that buffer passed must be 
        // allocated with malloc
        //
        m_pBuf = static_cast < char * > ( malloc ( m_bufSizeInit ) );
        if ( m_pBuf ) {
            m_bufSize = m_bufSizeInit; 
        }
        else {
            m_bufSize = 0u; 
            return pMangledName;
        }
    }
    int status = -1000;
    size_t sz = m_bufSize;
    char * const pBufResult =
            abi :: __cxa_demangle ( pMangledName, m_pBuf, 
                                        & sz, & status );
    if ( pBufResult ) {
        m_pBuf = pBufResult;
        m_bufSize = sz;
        if ( status == 0 ) {
            return string ( pBufResult );
        }
        else {
            return string ( pMangledName );
        }
    }
    return string ( pMangledName );
}

void ThreadPrivate :: epicsExit ( void * p )
{
    if ( m_privateId.m_epics ) {
        epicsThreadPrivateSet ( m_privateId.m_epics, 0 );
    }
    ThreadPrivate * const pPriv = static_cast < ThreadPrivate * > ( p );
    delete pPriv;
}

inline ThreadPrivate * ThreadPrivate :: get ()
{
    STATIC_ASSERT ( sizeof ( m_privateId.m_cmpAndSwap ) ==
                      sizeof ( m_privateId.m_epics ) );
    if ( ! m_privateId.m_epics ) {
        ThreadPrivateId newId;
        newId.m_epics = epicsThreadPrivateCreate ();
        if ( ! newId.m_epics ) {
            return 0;
        }
        const EpicsAtomicPtrT pBefore =
            epicsAtomicCmpAndSwapPtrT ( & m_privateId.m_cmpAndSwap,
                                        0, newId.m_cmpAndSwap );
        if ( pBefore ) {
            epicsThreadPrivateDelete ( newId.m_epics );
        }
    }
    void * const p = epicsThreadPrivateGet ( m_privateId.m_epics );
    ThreadPrivate * pPriv = static_cast < ThreadPrivate * > ( p );
    if ( ! pPriv ) {
        pPriv = new ( std :: nothrow ) ThreadPrivate ();
        if ( pPriv ) {
            const int status = 
                    epicsAtThreadExit ( ThreadPrivate :: epicsExit, pPriv );
            if ( status < 0 ) {
                delete pPriv;
                pPriv = 0;
            }
            epicsThreadPrivateSet ( m_privateId.m_epics, pPriv );
        }
    }
    return pPriv;
}

/**
 * Transforms a C++ ABI identifier into the original C++ source identifier.
 * @param pMangledName The name that shall be demangled.
 * @note This function tries to reduce the number of memory allocations by
 * reusing a thread-local buffer. If the length of the demangled string exceeds
 * the buffer size it might reallocate, though.
 */
std :: string epicsDemangle ( const char * const pMangledName )
{
    if ( pMangledName ) {
#       if ( __GNUC__ * 100 + __GNUC_MINOR__ ) >= 300
            ThreadPrivate * const pPriv = ThreadPrivate :: get ();
            if ( ! pPriv ) {
                return pMangledName;
            }
            return pPriv->demangle ( pMangledName );
#       else
            return pMangledName;
#       endif
    }
    else {
        return "<nil type name>";
    }
}

std :: string epicsDemangleTypeName ( const std :: type_info & ti )
{
    return epicsDemangle ( ti.name () );
}
