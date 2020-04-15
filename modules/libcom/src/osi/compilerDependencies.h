/*************************************************************************\
* Copyright (c) 2020 Triad National Security, as operator of Los Alamos 
*     National Laboratory
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Author: 
 * Jeffrey O. Hill
 * johill@lanl.gov
 */

#ifndef compilerDependencies_h
#define compilerDependencies_h

#include "compilerSpecific.h"

#ifdef __cplusplus

#if __cplusplus >= 201103L
#define epicsOverride override
#else
#define epicsOverride 
#endif

#if __cplusplus >= 201103L
#define epicsFinal final
#else
#define epicsFinal
#endif

#if __cplusplus >= 201103L
#define epicsNoexcept noexcept
#else
#define epicsNoexcept throw()
#endif

#if __cplusplus >= 201103L
#define epicsConstexpr constexpr
#else
#define epicsConstexpr 
#endif

#if __cplusplus >= 201103L
#define epicsMove(A) std::move(A)
#else
#define epicsMove(A) (A)
#endif

/*
 * deleted method should also be declared private as per convention
 * with old compilers
 */
#if __cplusplus >= 201103L
#define epicsDeleteMethod =delete
#else
#define epicsDeleteMethod 
#endif

/*
 * usage: epicsPlacementDeleteOperator (( void *, myMemoryManager & ))
 */
#if defined ( CXX_PLACEMENT_DELETE )
#   define epicsPlacementDeleteOperator(X) void operator delete X;
#else
#   define epicsPlacementDeleteOperator(X)
#endif

#endif /* __cplusplus */


#ifndef EPICS_PRINTF_STYLE
/*
 * No format-string checking
 */
#   define EPICS_PRINTF_STYLE(f,a)
#endif

#ifndef EPICS_DEPRECATED
/*
 * No deprecation markers
 */
#define EPICS_DEPRECATED
#endif

#ifndef EPICS_UNUSED
#   define EPICS_UNUSED
#endif

#ifndef EPICS_FUNCTION
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)) || (defined(__cplusplus) && __cplusplus>=201103L)
#  define EPICS_FUNCTION __func__
#else
/* Expands to a 'const char*' which describes the name of the current function scope */
#  define EPICS_FUNCTION ("<unknown function>")
#endif
#endif

#endif  /* ifndef compilerDependencies_h */
