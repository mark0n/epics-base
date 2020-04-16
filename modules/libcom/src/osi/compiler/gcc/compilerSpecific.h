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

#ifndef compilerSpecific_h
#define compilerSpecific_h

#ifndef __GNUC__
#   error compiler/gcc/compilerSpecific.h is only for use with the gnu compiler
#endif

#ifdef __clang__
#   error compiler/gcc/compilerSpecific.h is not for use with the clang compiler
#endif

#define EPICS_ALWAYS_INLINE __inline__ __attribute__((always_inline))

/* Expands to a 'const char*' which describes the name of the current function scope */
#define EPICS_FUNCTION __PRETTY_FUNCTION__

#define NO_RETURN __attribute__((noreturn))

#ifdef __cplusplus

/*
 * in general we dont like ifdefs but they do allow us to check the
 * compiler version and make the optimistic assumption that
 * standards incompliance issues will be fixed by future compiler
 * releases
 */

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 */
#define CXX_PLACEMENT_DELETE

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))

#if __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ >= 8 )
#   define USING_BASE_TYPE(B,T) using typename B :: T;
#else
#   define USING_BASE_TYPE(B,T) typedef typename B :: T T;
#endif

/*
 * Deprecation marker
 */
#define EPICS_DEPRECATED __attribute__((deprecated))

/*
 * Unused marker
 */
#define EPICS_UNUSED __attribute__((unused))

#endif  /* ifndef compilerSpecific_h */
