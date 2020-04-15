
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author Jeffrey O. Hill
 */
 
#include <cstdio>
#include <memory>

#define epicsExportSharedSymbols
#include "AllocatorArena.h"
#include "errlog.h"
#include "epicsTypes.h"
#include "epicsExit.h"
#include "epicsDemangle.h"

namespace epics {
namespace _impl {

const char * ThreadPrivateIdBadAlloc :: what () const throw () 
{
    return "epicsThreadPrivateCreate returned nill";
}

const char * AtThreadExitBadAlloc :: what () const throw () 
{
    return "epicsAtThreadExit was unsuccessful";
}

const size_t AllocCtxCom :: m_initialCapacity = 16u;

AllocCtxCom :: AllocCtxCom () :
    m_threadPrivateId ( epicsThreadPrivateCreate () ),
    m_curIdx ( 0 )
{
}

union RackManagerUnion {
    RackManager m_entry;
    size_t m_capacity;
};
    
size_t AllocCtxCom :: allocIdx ()
{
    return atomic :: increment ( m_curIdx );
}

/* 
 * from the bit twiddling hacks web site
 */
static inline epicsUInt32 nextPwrOf2 ( epicsUInt32 v )
{
    v--;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    v++;
    return v;
}

RackManager * AllocCtxCom :: getRackHandlerPtr ( const size_t idx )
{
    void * const pPriv = epicsThreadPrivateGet ( m_threadPrivateId );
    RackManagerUnion * pThrPriv;
    if ( pPriv ) {
        pThrPriv = static_cast < RackManagerUnion * > ( pPriv );
        if ( idx >= pThrPriv->m_capacity ) {
            const size_t newCapacity = nextPwrOf2 ( idx + 1u );
            RackManagerUnion * const pThrPrivNew = 
                                    new RackManagerUnion [newCapacity];
            assert ( newCapacity > idx );
            size_t i;
            for ( i = 1u; i < pThrPriv->m_capacity; i++ ) {
                pThrPrivNew[i] = pThrPriv[i];
            }
            for ( i = pThrPriv->m_capacity; i < newCapacity; i++ ) {
                pThrPrivNew[i].m_entry.m_pRack = 0;
                pThrPrivNew[i].m_entry.m_pThreadExitFunc = 0;
            }
            pThrPrivNew->m_capacity = newCapacity;
            delete [] pThrPriv;
            epicsThreadPrivateSet ( m_threadPrivateId, pThrPrivNew );
            pThrPriv = pThrPrivNew;
        }
    }
    else {
        size_t capacity;
        if ( idx >= m_initialCapacity ) {
            capacity = nextPwrOf2 ( idx + 1u );
        }
        else {
            capacity = m_initialCapacity;
        }
        pThrPriv = new RackManagerUnion [capacity];
        pThrPriv->m_capacity = capacity;
        for ( size_t i = 1u; i < capacity; i++ ) {
            pThrPriv[i].m_entry.m_pRack = 0;
            pThrPriv[i].m_entry.m_pThreadExitFunc = 0;
        }
        int status = epicsAtThreadExit ( m_threadExitFunc, this );
        if ( status != 0 ) {
            delete [] pThrPriv;
            throw AtThreadExitBadAlloc ();
        }
        epicsThreadPrivateSet ( m_threadPrivateId, pThrPriv );
    }
    return & pThrPriv[idx].m_entry;
}

void AllocCtxCom :: cleanup ( const size_t idx )
{
    void * const pPriv = epicsThreadPrivateGet ( m_threadPrivateId );
    if ( pPriv ) {
        RackManagerUnion * pThrPriv = 
                            static_cast < RackManagerUnion * > ( pPriv );
        if ( idx < pThrPriv->m_capacity ) {
            if ( pThrPriv[idx].m_entry.m_pThreadExitFunc && 
                                            pThrPriv[idx].m_entry.m_pRack ) {
                ( *pThrPriv[idx].m_entry.m_pThreadExitFunc )
                                    ( pThrPriv[idx].m_entry.m_pRack );
                pThrPriv[idx].m_entry.m_pRack = 0;
                pThrPriv[idx].m_entry.m_pThreadExitFunc = 0;
            }
        }
    }
}

void AllocCtxCom :: m_threadExitFunc ( void * const pPriv ) 
{
    AllocCtxCom * const pCtx = static_cast < AllocCtxCom * > ( pPriv );
    void * const pThrPrivVoid = epicsThreadPrivateGet ( pCtx->m_threadPrivateId );
    if ( pThrPrivVoid ) {
        RackManagerUnion * const pThrPriv = 
                        static_cast < RackManagerUnion * > ( pThrPrivVoid );
        for ( size_t i = 1u; i < pThrPriv->m_capacity; i++ ) {
            if ( pThrPriv[i].m_entry.m_pThreadExitFunc && 
                                            pThrPriv[i].m_entry.m_pRack ) {
                ( *pThrPriv[i].m_entry.m_pThreadExitFunc )
                                    ( pThrPriv[i].m_entry.m_pRack );
            }
        }
        delete [] pThrPriv;
        epicsThreadPrivateSet ( pCtx->m_threadPrivateId, 0 );
    }
}

AllocCounter < true > :: AllocCounter ()
{
    atomic :: set ( m_nRacks, 0u );
    atomic :: set ( m_bytes, 0u );
    atomic :: set ( m_nRacksTrace, 8 );
}

static size_t m_nRacksTotal = 0u;
static size_t m_bytesTotal = 0u;

void AllocCounter < true > :: increment ( size_t nBytesThisTime,
                                            const type_info & ti )
{
    const size_t newNRacksVal = atomic :: increment ( m_nRacks );
    atomic :: increment ( m_nRacksTotal );
    atomic :: add ( m_bytes, nBytesThisTime );
    atomic :: add ( m_bytesTotal, nBytesThisTime );
    if ( newNRacksVal >= m_nRacksTrace ) {
        atomic :: add ( m_nRacksTrace, m_nRacksTrace );
        this->show ( ti );
    }
}

void AllocCounter < true > :: decrement ( size_t nBytesThisTime )
{
    atomic :: decrement ( m_nRacks );
    atomic :: decrement ( m_nRacksTotal );
    atomic :: subtract ( m_bytes, nBytesThisTime );
    atomic :: subtract ( m_bytesTotal, nBytesThisTime );
}

void AllocCounter < true > :: show ( const type_info & ti ) const
{
    const std :: string name = epicsDemangleTypeName ( ti );
    errlogPrintf ( 
            "AA C=%08lu SZ=%08lu CT=%08lu SZT=%08lu \"%s\"\n", 
            ( unsigned long ) m_nRacks,
            ( unsigned long ) m_bytes,
            ( unsigned long ) m_nRacksTotal,
            ( unsigned long ) m_bytesTotal,
            name.c_str () );
}

} // end of name space _impl
} // end of name space epics
