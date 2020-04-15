/*************************************************************************\
 * Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
 *     National Laboratory
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsMemChk_h
#define epicsMemChk_h 


#if defined ( __cplusplus ) 
extern "C" {
#endif 

#ifdef MEMCHECK 
#   include "valgrind/memcheck.h"
    typedef struct epicsMemChkRedZone { volatile char m_redZone[32]; } epicsMemChkRedZone;
#   define VALGRIND_RED_ZONE(memberName) epicsMemChkRedZone memberName;
#   define VALGRIND_RED_ZONE_SIZE ( sizeof ( epicsMemChkRedZone ) )
#else
#   define VALGRIND_RED_ZONE(memberName) 
#   define VALGRIND_RED_ZONE_SIZE 0 
#   define VALGRIND_MAKE_MEM_NOACCESS(_qzz_addr, _qzz_len)
#   define VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr, _qzz_len)
#   define VALGRIND_MAKE_MEM_DEFINED(_qzz_addr, _qzz_len)
#   define VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed)
#   define VALGRIND_CREATE_MEMPOOL_EXT(pool, rzB, is_zeroed, flags)
#   define VALGRIND_DESTROY_MEMPOOL(pool)
#   define VALGRIND_MEMPOOL_ALLOC(pool, addr, sz)
#   define VALGRIND_MEMPOOL_FREE(pool, addr)
#   define VALGRIND_MALLOCLIKE_BLOCK(addr, sizeB, rzB, is_zeroed)
#   define VALGRIND_FREELIKE_BLOCK(addr, rzB)
#   define VALGRIND_MEMPOOL_METAPOOL 0
#endif

#if defined ( __cplusplus ) 
} // end of extern C 
#endif 

#endif // ifdef epicsMemChk_h

