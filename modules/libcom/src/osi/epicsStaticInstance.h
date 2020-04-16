/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsStaticInstance_h
#define epicsStaticInstance_h

namespace epics {

//
// c++ 11 specifies the behavior for concurrent
// access to block scope statics but some compiler
// writers, lacking clear guidance in the earlier
// c++ standards, curiously implement thread unsafe
// block static variables despite ensuring for
// proper multi-threaded behavior for many other
// aspects of the compiler runtime infrastructure
// such as runtime support for exception handling
//
// T - the type of the initialized once only static
//
template < typename T > T & staticInstance ();

//
// !!!! perhaps we need versions here that pass parameters 
// !!!! to the constructor using templates also
//

} // end of name space epics

//
// if the compiler claims its c++ 11 then we are optimistic, ref:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3242.pdf
// (see section 6.7)
//
#if __cplusplus >= 201103L
#   include "epicsStaticInstanceSaneCmplr.h"
#else
    // otherwise we can implement compiler specific behavior
#   include "epicsStaticInstanceCD.h"
#endif

#endif // ifndef epicsStaticInstance_h

