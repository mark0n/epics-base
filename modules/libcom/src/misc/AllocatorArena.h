
/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author Jeffrey O. Hill
 */

//
// Arena Allocator
//
// A thread private allocator for N of type T allocated in-mass, and
// deallocated in-mass. The individual calls to allocate run very
// efficiently because they simply return a pointer to preallocated
// space for a type T, and advance the index identifying the space for
// the next allocation, using thread private storage sans mutex
// protection overhead. The in-bulk deallocation is postponed until 
// the last active T, residing within a bulk allocated block, is 
// deallocated.
//
// The allocator falls back to ordinary global new/delete if the user 
// requests more than one element for allocation using the vector 
// form of new.
//
// This facility is fully safe for use in a multi-threaded environment.
// A thread private variable, and a thread private cleanup, are used to
// maintain a thread private context so that the overhead associated
// with a mutual exclusion locking can be avoided.
//
// Be aware that the storage overhead for any type T is sizeof ( T ) plus 
// sizeof ( void * ), a substantial consideration probably only if small 
// objects are stored, and storage efficiency is more important than
// performance considerations. Furthermore, storage overhead increases 
// for C++ compilers predating C++ 11.
//
// In this code a contiguous bulk block of storage for N of type T 
// is template type Rack<S,A,N> where S is the size of T, and A is the 
// required alignment. The thread-private allocation occurs when peeling
// off storage for an individual T from the thread's private Rack. When 
// the thread's private Rack is exhausted, then a new Rack is allocated
// from the specified Rack allocator type. Currently two global Rack 
// allocator implementations are provided. One that uses ordinary global 
// new/delete, and one that uses a mutex protected global free list 
// (the default) designated by rack allocator policies rap_pool and 
// rap_freeList respectively.
//
// Define ALLOCATOR_ARENA_MEMORY_ACCESS_INSPECTION_ACTIVE in order to
// temporarily bypass bulk allocation and return to ordinary global
// operator new and delete, so that memory access inspectors such as 
// purify or valgrind might be more effective. Furthermore, when the 
// allocator _is_ active, and it is a debug build, specialized
// valgrind macros document with valgrind memory regions owned by this 
// allocator for which ownership has not yet passed to an end application
// so that they do not appear as undesirable noise in valgrind leak
// reports.
//

#ifndef epicsAllocatorArena_h
#define epicsAllocatorArena_h

// see comment above
#if 0
#define ALLOCATOR_ARENA_MEMORY_ACCESS_INSPECTION_ACTIVE
#endif

#if __cplusplus >= 201103L
#   include <cstdint>
#endif

#include <new>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <typeinfo>

#include "epicsAtomic.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsStaticInstance.h"
#include "compilerDependencies.h"
#include "valgrind/epicsMemChk.h"
#include "shareLib.h"


#if __cplusplus >= 201103L
#   if defined ( __GNUC__ ) 
#       define GCC_VERSION_AA \
            (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
        // perhaps the nios2 cross compiler doesn't properly implement
        // alignment even at gcc 4.8, based on experimental evidence
#       define ALIGNAS_CMPLR_OK ( GCC_VERSION_AA >= 50000 )
#   else
#       define ALIGNAS_CMPLR_OK 1
#   endif
#else
#   define ALIGNAS_CMPLR_OK 0
#endif

#ifndef SIZE_MAX
#   define SIZE_MAX ( static_cast < size_t > ( -1L ) )
#endif

namespace epics {
namespace _impl {

using std :: size_t;
using std :: type_info;
using std :: ptrdiff_t;
using std :: allocator;
using std :: bad_alloc;

//
// S - the allocation size (the size of T)
// A - the required alignment
// N - the block size for pool allocation 
//     (pool allocation size will be approximately S * N)
//
template < size_t S, size_t A, size_t N = 256 > class Rack;

//
// R - the rack type
// P - the allocation policy type
// TRACE - if true, print a message each time that a new racks allocated
//         counter power of two is exceeded for a particular allocation type
//
enum RackAllocPolicy { rap_pool, rap_freeList };
template < typename R, RackAllocPolicy P, bool TRACE > class RackAlloc;

//
// T - the allocation type
// G - the discriminator for the thread private variable group
//     (proper scaling requires an independent thread private variable
//      group for each library) any type is allowed and appropriate for
//      identifying an independent thread private context
// N - the block size for pool allocation 
// A - the rack allocator
//
// on old compilers prior to ALIGNAS_CMPLR_OK all blocks are aligned 
// for worst case alignment requirements
//
template < typename T, typename G, size_t N = 256, 
            RackAllocPolicy P = rap_freeList, bool TRACE = false >
class AllocatorArena {
public:
    typedef T value_type;
    typedef value_type * pointer;
    typedef const value_type * const_pointer;
    typedef value_type & reference;
    typedef const value_type & const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef typename allocator < void > :: const_pointer void_const_pointer;
    typedef typename allocator < void > :: pointer void_pointer;

    template < typename T0 >
    struct rebind {
        typedef AllocatorArena < T0, G, N, P, TRACE > other;
    };

    AllocatorArena ();

    template < typename Y >
    AllocatorArena ( const Y & );

    //    address
    static pointer address ( reference r );
    static const_pointer address ( const_reference r );

    //    memory allocation
    static pointer allocate ( size_type nAlloc,
                                void_const_pointer pNearbyHint = 0 );
    static void deallocate ( const pointer p, size_type nAlloc );

    // helpers, for creating class specific operator new and delete
    static void * allocateOctets ( size_type size );
    static void * allocateOctets ( size_type size, const std :: nothrow_t & );
    static void deallocateOctets ( void * p, size_type size );

    // remove the allocator's thread private reference to any partially
    // consumed block so that it can be released back into system pool
    // (this action is automated at thread exit)
    static void cleanup ();

    //    size
    static size_type max_size ();

    // construction / destruction of target
#if __cplusplus >= 201103L  
    template < typename T0, typename ... Args >
    static void construct ( T0 *, Args && ... ); // deprecated in c++ 17
#else
    template < typename T0, typename A0 >
    static void construct ( T0 *, A0 ); 
    template < typename T0, typename A0, typename A1 >
    static void construct ( T0 *, A0, A1 ); 
    template < typename T0, typename A0, typename A1, typename A2 >
    static void construct ( T0 *, A0, A1, A2 ); 
    template < typename T0, typename A0, typename A1, typename A2, 
                            typename A3 >
    static void construct ( T0 *, A0, A1, A2, A3 ); 
    template < typename T0, typename A0, typename A1, typename A2, 
                            typename A3, typename A4 >
    static void construct ( T0 *, A0, A1, A2, A3, A4 ); 
#endif
    template < class U >
    static void destroy ( U * ); // depricated in c++ 17

    bool operator == ( AllocatorArena const & );
    bool operator != ( AllocatorArena const & );

    static size_t rackCount ();
    static size_t byteCount ();
private:
#if ALIGNAS_CMPLR_OK
    typedef Rack < sizeof ( T ), alignof ( T ), N > M_Rack;
#else
    /* worst case alignment is used */
    typedef Rack < sizeof ( T ), 0u, N > M_Rack;
#endif
    typedef RackAlloc < M_Rack, P, TRACE > M_RackAlloc;
    static pointer m_threadPrivateAlloc ();
    static void m_rackCleanup ( void * );
}; // end of class AllocatorArena

class epicsShareClass ThreadPrivateIdBadAlloc : public bad_alloc 
{
    const char * what () const throw ();
};

class epicsShareClass AtThreadExitBadAlloc : public bad_alloc 
{
    const char * what () const throw ();
};

#if __cplusplus >= 201103L
    typedef std :: uint8_t Octet;
#else
    typedef unsigned char Octet;
#endif

typedef Octet * POctet;

// align for all possible types, similar to malloc
union MaxAlign {
    struct SomeStruct {};
    long double m_ld;
    long long m_ll;
    long double * m_pd;
    SomeStruct * m_pss;
    void (* m_pf) ();
    long double SomeStruct :: * m_pmd;
    void ( SomeStruct :: * m_pmf ) ();
    /*
     * eye of newt ...
     */
};

template < size_t S, size_t A, size_t N >
class Rack {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    Rack ();
    ~Rack ();
    bool empty () const;
    void * alloc ();
    size_t removeReference ();
    void addReference ();

    static const size_t number;
    static const size_t alignment;
    static Rack * dealloc ( void * );
private:
    struct M_Wrapper {
        VALGRIND_RED_ZONE ( m_redZoneBefore )
#       if ALIGNAS_CMPLR_OK
            alignas ( A ) Octet m_bufForObj [ S ];
#       else
            union {
                Octet m_bufForObj [ S ];
                MaxAlign m_maxAlign;
            };
#       endif
        VALGRIND_RED_ZONE ( m_redZoneAfter )
        void * m_pRack;
    };
    typedef M_Wrapper * PWrapper;
    M_Wrapper m_wrapped[N];
    size_t m_nAlloc;
    size_t m_refCount;
#if __cplusplus >= 201103L
    Rack ( const Rack & ) = delete;
    Rack & operator = ( const Rack & ) = delete;
#endif
};

struct RackManager {
    void * m_pRack;
    void ( *m_pThreadExitFunc )( void * pRack );
};

class epicsShareClass AllocCtxCom {
public:
    AllocCtxCom ();
    RackManager * getRackHandlerPtr ( size_t idx );
    void cleanup ( size_t idx );
    size_t allocIdx ();
private:
    epicsThreadPrivateId m_threadPrivateId;
    size_t m_curIdx;
    static void m_threadExitFunc ( void * const pPriv );
    static const size_t m_initialCapacity;
};

//
// G - the discriminator for the thread private variable group
//     (proper scaling requires an independent thread private variable
//      group for each library) any type is allowed and appropriate 
//      for identifying an independent thread private context
//
template < typename G > class AllocCtxGrouped : public AllocCtxCom {};

//
// G - the discriminator for the thread private variable group
//     (proper scaling requires an independent thread private variable
//      group for each library) any type is allowed and appropriate 
//      for identifying an independent thread private context
//
template < typename G >
class AllocCtx {
public:
    AllocCtx ();
    void cleanup ();
    RackManager * getRackHandlerPtr ();
private:
    size_t m_idx;
};

//
// T - the allocation type
// G - the discriminator for the thread private variable group
//     (proper scaling requires an independent thread private variable
//      group for each library) any type is allowed and appropriate 
//      for identifying an independent thread private context
//
template < typename T, typename G > 
class AllocCtxTyped : public AllocCtx < G > {};

template < bool TRACE >
class epicsShareClass AllocCounter;

template <>
class epicsShareClass AllocCounter < true > {
public:
    AllocCounter ();
    void increment ( size_t nBytesThisTime, const type_info & );
    void decrement ( size_t nBytesThisTime );
    size_t rackCount () const { return m_nRacks; }
    size_t byteCount () const { return m_bytes; }
    void show ( const type_info & ) const;
private:
    size_t m_nRacksTrace;
    size_t m_nRacks;
    size_t m_bytes;
};

template <>
class epicsShareClass AllocCounter < false > {
public:
    void increment ( size_t nBytesThisTime,
                        const type_info & ) {}
    void decrement ( size_t nBytesThisTime ) {}
    size_t rackCount () const { return 0u; }
    size_t byteCount () const { return 0u; }
    void show ( const char * pContext ) const {}
};

template < typename R, bool TRACE >
class RackAlloc < R, rap_pool, TRACE > {
public:
    typedef R Rack;
    template < typename R0 >
    struct rebind {
        typedef RackAlloc < R0, rap_pool, TRACE > other;
    };
    static R * create ( const type_info & ti );
    static void destroy ( R * p );
    static size_t rackCount ();
    static size_t byteCount ();
private:
    static AllocCounter < TRACE > m_counter;
};

template < typename R, bool TRACE >
class RackAlloc < R, rap_freeList, TRACE > {
public:
    template < typename R0 >
    struct rebind {
        typedef RackAlloc < R0, rap_freeList, TRACE > other;
    };
    typedef R Rack;
    RackAlloc ();
    ~RackAlloc ();
    R * create ( const type_info & );
    void destroy ( R * p );
    size_t rackCount () const;
    size_t byteCount () const;
private:
    typedef epicsMutex Mutex;
    typedef epicsGuard < epicsMutex > Guard;
    struct M_Alias {
        VALGRIND_RED_ZONE ( m_redZoneBefore )
        union {
#           if ALIGNAS_CMPLR_OK
                alignas ( R :: alignment ) 
                    Octet m_rackBuf [ sizeof ( R ) ];
#           else
                union {
                    Octet m_rackBuf [ sizeof ( R ) ];
                    MaxAlign m_maxAlign;
                };  
#           endif
            struct M_Alias * m_pNext;
        };
        VALGRIND_RED_ZONE ( m_redZoneAfter )
    };
    M_Alias * m_pAlias;
    Mutex m_mutex;
    AllocCounter < TRACE > m_counter;
    RackAlloc ( const RackAlloc & ) epicsDeleteMethod;
    RackAlloc operator = ( const RackAlloc & ) epicsDeleteMethod;
};

template < typename G >
inline AllocCtx < G > :: AllocCtx () :
    m_idx ( staticInstance < AllocCtxGrouped < G > > ().allocIdx () )
{ 
}

template < typename G >
inline RackManager * AllocCtx < G > :: getRackHandlerPtr () 
{ 
    AllocCtxCom & grp = staticInstance < AllocCtxGrouped < G > > ();
    return grp.getRackHandlerPtr ( m_idx );
}

template < typename G >
void AllocCtx < G > :: cleanup () 
{ 
    AllocCtxCom & grp = staticInstance < AllocCtxGrouped < G > > ();
    grp.cleanup ( m_idx );
}

template < typename R, bool TRACE >
inline R * RackAlloc < R, rap_pool, TRACE > :: create ( const type_info & ti )
{
    R * const p = new R ();
    m_counter.increment ( sizeof ( R ), ti );
    return p;
}

template < typename R, bool TRACE >
inline void RackAlloc < R, rap_pool, TRACE > :: destroy ( R * p )
{
    m_counter.decrement ( sizeof ( R ) );
    delete p;
}

template < typename R, bool TRACE >
inline size_t RackAlloc < R, rap_pool, TRACE > :: rackCount ()
{ 
    return m_counter.rackCount (); 
}

template < typename R, bool TRACE >
inline size_t RackAlloc < R, rap_pool, TRACE > :: byteCount ()
{ 
    return m_counter.byteCount (); 
}

template < typename R, bool TRACE >
inline RackAlloc < R, rap_freeList, TRACE > :: RackAlloc () : 
    m_pAlias ( 0 )
{
    VALGRIND_CREATE_MEMPOOL_EXT ( this, sizeof (epicsMemChkRedZone), 
                                false, VALGRIND_MEMPOOL_METAPOOL );
}

template < typename R, bool TRACE >
RackAlloc < R, rap_freeList, TRACE > :: ~RackAlloc ()
{
    Guard guard ( m_mutex );
    M_Alias * p = m_pAlias;
    while ( p ) {
        M_Alias * pNext = p->m_pNext;
        {
            delete p;
        }
        p = pNext;
    };
    VALGRIND_DESTROY_MEMPOOL ( this );
}

template < typename R, bool TRACE >
R * RackAlloc < R, rap_freeList, TRACE > :: create ( 
                                        const type_info & ti )
{
    M_Alias * pA = 0;
    {
        Guard guard ( m_mutex );
        if ( m_pAlias ) {
            pA = m_pAlias;
            m_pAlias = m_pAlias->m_pNext;
        }
    }
    if ( ! pA ) {
        pA = new M_Alias;
        VALGRIND_MAKE_MEM_NOACCESS ( pA, sizeof ( *pA ) );
    }
    VALGRIND_MEMPOOL_ALLOC ( this, pA->m_rackBuf, 
                                        sizeof ( pA->m_rackBuf ) );
    m_counter.increment ( sizeof ( R ), ti );

    return new ( pA->m_rackBuf ) R ();
}

template < typename R, bool TRACE >
void RackAlloc < R, rap_freeList, TRACE > :: destroy ( R * const pR )
{
    if ( pR ) {
        pR->~R ();
        static const size_t bufOffset = offsetof ( M_Alias, m_rackBuf );
        const POctet pOctets = reinterpret_cast < POctet > ( pR ) - bufOffset;
        M_Alias * const pA = reinterpret_cast < M_Alias * > ( pOctets );
        VALGRIND_MEMPOOL_FREE ( this, pA->m_rackBuf );
        VALGRIND_MAKE_MEM_UNDEFINED ( &pA->m_pNext, sizeof ( pA->m_pNext ) );
        {
            Guard guard ( m_mutex );
            pA->m_pNext = m_pAlias;
            m_pAlias = pA;
        }
        VALGRIND_MAKE_MEM_DEFINED ( &pA->m_pNext, sizeof ( pA->m_pNext ) );
        m_counter.decrement ( sizeof ( R ) );
    }
}

template < typename R, bool TRACE >
inline size_t RackAlloc < R, rap_freeList, TRACE > :: rackCount () const
{ 
    return m_counter.rackCount (); 
}

template < typename R, bool TRACE >
inline size_t RackAlloc < R, rap_freeList, TRACE > :: byteCount () const
{ 
    return m_counter.byteCount (); 
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline AllocatorArena < T, G, N, P, TRACE > :: AllocatorArena ()
{
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename Y >
inline AllocatorArena < T, G, N, P, TRACE > :: AllocatorArena ( const Y & ) 
{
}

//    address
template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline typename AllocatorArena < T, G, N, P, TRACE > :: pointer
    AllocatorArena < T, G, N, P, TRACE > :: address ( reference r )
{
    return & r;
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline typename AllocatorArena < T, G, N, P, TRACE > :: const_pointer
    AllocatorArena < T, G, N, P, TRACE > :: address ( const_reference r )
{
    return & r;
}

//
//    memory allocation
//    (inline arranges for pNearbyHint to be eliminated 
//      at compile time)
template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline typename AllocatorArena < T, G, N, P, TRACE > :: pointer
    AllocatorArena < T, G, N, P, TRACE > :: allocate ( size_type nAlloc,
                                        void_const_pointer pNearbyHint )
{
    pointer p;
    if ( nAlloc == 1u ) {
#if defined ( ALLOCATOR_ARENA_MEMORY_ACCESS_INSPECTION_ACTIVE )
#       if __cplusplus >= 201703L
            void * const pVoid = :: operator new ( sizeof (T), 
                                                    alignof ( T ) );
#       else
            // This function is required to return a pointer suitably 
            // aligned to hold an object of any fundamental alignment.
            void * const pVoid = :: operator new ( sizeof (T) );
#       endif
        p = static_cast < pointer > ( pVoid );
#else
        p = m_threadPrivateAlloc ();
#endif
    }
    else {
#       if __cplusplus >= 201703L
            void * const pVoid = :: operator new ( sizeof (T) * nAlloc, 
                                                    alignof ( T ) );
#       else
            // This function is required to return a pointer suitably 
            // aligned to hold an object of any fundamental alignment.
            void * const pVoid = :: operator new ( sizeof (T) * nAlloc );
#       endif
        p = static_cast < pointer > ( pVoid );
    }
    return p;
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline void AllocatorArena < T, G, N, P, TRACE > :: deallocate ( const pointer p,
                                                                size_type nAlloc )
{
    if ( nAlloc == 1u ) {
#if defined ( ALLOCATOR_ARENA_MEMORY_ACCESS_INSPECTION_ACTIVE )
        :: operator delete ( p );
#else
        M_Rack * const pRack = M_Rack :: dealloc ( p );
        if ( pRack ) {
            staticInstance < M_RackAlloc > ().destroy ( pRack );
        }
#endif
    }
    else {
        :: operator delete ( p );
    }
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline void * AllocatorArena < T, G, N, P, TRACE > :: 
                                allocateOctets ( size_type sz )
{
    if ( sz == sizeof ( T ) ) {
        return AllocatorArena :: allocate ( 1u );
    }
    else {
        // This function is required to return a pointer suitably 
        // aligned to hold an object of any fundamental alignment.
        return :: operator new ( sz );
    }
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline void * AllocatorArena < T, G, N, P, TRACE > :: 
                                allocateOctets ( size_type sz, 
                                        const std :: nothrow_t & )
{
    if ( sz == sizeof ( T ) ) {
        try {
            return AllocatorArena :: allocate ( 1u );
        }
        catch ( const std :: bad_alloc & ) {
            return static_cast < void * > ( 0 );
        }
    }
    else {
        // This function is required to return a pointer suitably 
        // aligned to hold an object of any fundamental alignment.
        return :: operator new ( sz, std :: nothrow );
    }
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline void AllocatorArena < T, G, N, P, TRACE > :: 
                            deallocateOctets ( void * const p, size_type sz )
{
    if ( sz == sizeof ( T ) ) {
        T * const pT = static_cast < T * > ( p );
        AllocatorArena :: deallocate ( pT, 1u );
    }
    else {
#if __cplusplus >= 201400L
        :: operator delete ( p, sz );
#else
        :: operator delete ( p );
#endif
    }
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline typename AllocatorArena < T, G, N, P, TRACE > :: size_type
    AllocatorArena < T, G, N, P, TRACE > :: max_size ()
{
    // This class is intended only for use with an allocation size of one;
    // if requests for more than one object occur it falls back to ordinary
    // new and delete based allocation.
    return 1u;
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
typename AllocatorArena < T, G, N, P, TRACE > :: pointer
    AllocatorArena < T, G, N, P, TRACE > :: m_threadPrivateAlloc ()
{
    AllocCtx < G > & ctx = staticInstance < AllocCtxTyped < T, G > > ();
    RackManager * const pRM = ctx.getRackHandlerPtr ();
    M_Rack * pRack = static_cast < M_Rack * > ( pRM->m_pRack );
    if ( pRack ) {
        pointer p = static_cast < T * > ( pRack->alloc () );
        if ( p ) {
            if ( pRack->empty () ) {
                assert ( pRM->m_pThreadExitFunc );
                ( *pRM->m_pThreadExitFunc ) ( pRack );
                pRM->m_pThreadExitFunc = 0;
                pRM->m_pRack = 0;
            }
            return p;
        }
        assert ( pRM->m_pThreadExitFunc );
        ( *pRM->m_pThreadExitFunc ) ( pRack );
        pRM->m_pThreadExitFunc = 0;
        pRM->m_pRack = 0;
    }
    pRack = staticInstance < M_RackAlloc > ().create ( typeid ( T ) );
    assert ( pRack );
    pRack->addReference ();
    pRM->m_pThreadExitFunc = m_rackCleanup;
    pRM->m_pRack = pRack;
    pointer p = static_cast < pointer > ( pRack->alloc () );
    assert ( p );
    return p;
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
void AllocatorArena < T, G, N, P, TRACE > :: m_rackCleanup ( void * const pPriv )
{
    assert ( pPriv );
    M_Rack * pRack = static_cast < M_Rack * > ( pPriv );
    if ( pRack->removeReference () == 0u ) {
        staticInstance < M_RackAlloc > ().destroy ( pRack );
    }
}

#if __cplusplus >= 201103L
template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename ... Args >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct ( T0 * p, 
                                                Args && ... args )
{
    void * const pvoid = p;
    :: new ( pvoid ) T0 ( std :: forward <Args> ( args ) ... );
}
#else

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename A0 >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct (
                                    T0 * p, A0 a0 )
{
    new ( p ) T ( a0 );
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename A0, typename A1 >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct (
                                    T0 * p, A0 a0, A1 a1 )
{
    new ( p ) T ( a0, a1 );
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename A0, typename A1, typename A2 >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct (
                                    T0 * p, A0 a0, A1 a1, A2 a2 )
{
    new ( p ) T ( a0, a1, a2 );
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename A0, typename A1, typename A2,
            typename A3 >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct (
                                    T0 * p, A0 a0, A1 a1, A2 a2, A3 a3 )
{
    new ( p ) T ( a0, a1, a2, a3 );
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < typename T0, typename A0, typename A1, typename A2,
            typename A3, typename A4 >
inline void AllocatorArena < T, G, N, P, TRACE > :: construct (
                                    T0 * p, A0 a0, A1 a1, A2 a2, A3 a3, 
                                    A4 a4 )
{
    new ( p ) T ( a0, a1, a2, a3, a4 );
}

#endif

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
template < class U >
inline void AllocatorArena < T, G, N, P, TRACE > :: destroy ( U * p )
{
    p->~U();
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline bool AllocatorArena < T, G, N, P, TRACE > :: operator == (
                                        AllocatorArena const & ao )
{
    return true;
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline bool AllocatorArena < T, G, N, P, TRACE > :: operator != (
                                        AllocatorArena const & a )
{
    return ! operator == ( a );
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline void AllocatorArena < T, G, N, P, TRACE > :: cleanup ()
{
    AllocCtx < G > & ctx = staticInstance < AllocCtxTyped < T, G > > ();
    ctx.cleanup ();
}
 
template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline size_t AllocatorArena < T, G, N, P, TRACE > :: rackCount ()
{
    return staticInstance < M_RackAlloc > ().rackCount();
}

template < typename T, typename G, size_t N, RackAllocPolicy P, bool TRACE >
inline size_t AllocatorArena < T, G, N, P, TRACE > :: byteCount ()
{
    return staticInstance < M_RackAlloc > ().byteCount();
}

template < size_t S, size_t A, size_t N >
const size_t Rack < S, A, N > :: number = N;

template < size_t S, size_t A, size_t N >
const size_t Rack < S, A, N > :: alignment = A;

template < size_t S, size_t A, size_t N >
inline Rack < S, A, N > :: Rack () :
    m_nAlloc ( 0u ),
    m_refCount ( 0u )
{
    VALGRIND_MAKE_MEM_NOACCESS ( m_wrapped, sizeof ( m_wrapped ) );
}

template < size_t S, size_t A, size_t N >
inline Rack < S, A, N > :: ~Rack ()
{
    // if a Rack allocating free list is destroyed, and it destroys 
    // a rack with outstanding references remaining then  problems 
    // will ensue 
    assert ( m_refCount == 0u );
    VALGRIND_MAKE_MEM_NOACCESS ( m_wrapped, sizeof ( m_wrapped ) );
}

template < size_t S, size_t A, size_t N >
inline bool Rack < S, A, N > :: empty () const
{
    return m_nAlloc >= N;
}

template < size_t S, size_t A, size_t N >
void * Rack < S, A, N > :: alloc ()
{
    void * p = 0;
    if ( m_nAlloc < N ) {
        M_Wrapper * const pAlloc = & m_wrapped[m_nAlloc];
        m_nAlloc++;
        this->addReference ();
        VALGRIND_MAKE_MEM_UNDEFINED ( &pAlloc->m_pRack, 
                                    sizeof ( pAlloc->m_pRack ) );
        atomic :: set ( pAlloc->m_pRack, this );
        VALGRIND_MAKE_MEM_DEFINED ( &pAlloc->m_pRack,
                                    sizeof ( pAlloc->m_pRack ) );
        p = static_cast < void * > ( pAlloc->m_bufForObj );
        VALGRIND_MALLOCLIKE_BLOCK ( pAlloc->m_bufForObj, 
                                    sizeof ( pAlloc->m_bufForObj ),
                                    sizeof ( epicsMemChkRedZone ),
                                    false );
    }
    return p;
}

template < size_t S, size_t A, size_t N >
Rack < S, A , N > * Rack < S, A, N > :: dealloc ( void * const p )
{
    static const size_t bufOffset = offsetof ( M_Wrapper, m_bufForObj );
    const POctet pOctets = static_cast < POctet > ( p ) - bufOffset; 
    const PWrapper pDealloc = reinterpret_cast < PWrapper > ( pOctets );
    VALGRIND_FREELIKE_BLOCK ( pDealloc->m_bufForObj,
                                sizeof ( epicsMemChkRedZone ) );
    Rack * const pRack = static_cast < Rack * > 
                            ( atomic :: get ( pDealloc->m_pRack ) );
    pDealloc->m_pRack = 0; // for imperfect error detection purposes 
    assert ( pRack );
    if ( pRack->removeReference () == 0u ) {
        return pRack;
    }
    return 0;
}

template < size_t S, size_t A, size_t N >
inline void Rack < S, A, N > :: addReference ()
{
    assert ( m_refCount < SIZE_MAX );
    atomic :: increment ( m_refCount );
}

template < size_t S, size_t A, size_t N >
inline size_t Rack < S, A, N > :: removeReference ()
{
    assert ( m_refCount > 0u );
    return atomic :: decrement ( m_refCount );
}

} // end of name space _impl

using _impl :: Rack;
using _impl :: RackAlloc;
using _impl :: RackAllocPolicy;
using _impl :: rap_pool;
using _impl :: rap_freeList;
using _impl :: AllocatorArena;

} // end of name space epics

#endif // if not defined epicsAllocatorArena_h

