
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

#ifndef epicsStaticInstanceSaneCmplr_h
#define epicsStaticInstanceSaneCmplr_h

namespace epics {

// c++ 11 specifies the behavior for concurrent
// access to block scope statics but some compiler
// writers, lacking clear guidance in the earlier
// c++ standards, curiously implement thread unsafe
// block static variables despite ensuring for
// proper multi-threaded behavior for many other
// aspects of the compiler infrastructure such as
// runtime support for exception handling
//
// if the compiler claims its c++ 11 then we are optimistic, ref:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3242.pdf
// (see section 6.7)
// "If control enters the (block-scope variable with static storage duration)
// declaration concurrently while the variable is being initialized, the 
// concurrent execution shall wait for completion of the initialization."
// 
// also see c++ FAQ, how do I prevent the
// "static initialization order fiasco"?
template < typename T > T & staticInstance ()
{
    static T * const m_pInstance = new T;
    return * m_pInstance;
}

} // end of name space epics

#endif // ifndef epicsStaticInstanceSaneCmplr_h
