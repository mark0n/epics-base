/*************************************************************************\
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

#if __GNUC__ > 2
#  define EPICS_ALWAYS_INLINE __inline__ __attribute__((always_inline))
#else
#  define EPICS_ALWAYS_INLINE __inline__
#endif
 
#ifdef __cplusplus

/*
 * in general we dont like ifdefs but they do allow us to check the
 * compiler version and make the optimistic assumption that 
 * standards incompliance issues will be fixed by future compiler 
 * releases
 */

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */

#define EPICS_GCC_VERSION (__GNUC__ * 10000 \
                           + __GNUC_MINOR__ * 100 \
                           + __GNUC_PATCHLEVEL__)

#if EPICS_GCC_VERSION >= 29500
#   define CXX_THROW_SPECIFICATION
#endif

#if EPICS_GCC_VERSION >= 29600
#   define CXX_PLACEMENT_DELETE
#endif

#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L)
#  define EPICS_GCC_CXX11
#endif

// https://en.cppreference.com/w/cpp/language/noexcept
#if (EPICS_GCC_VERSION >= 40600) && defined(EPICS_GCC_CXX11)
#  define EPICS_CXX11_NOEXCEPT noexcept
#else
#  define EPICS_CXX11_NOEXCEPT
#endif

// https://en.cppreference.com/w/cpp/language/final
// https://en.cppreference.com/w/cpp/language/override
#if (EPICS_GCC_VERSION >= 40700) && defined(EPICS_GCC_CXX11)
#  define EPICS_CXX11_FINAL final
#  define EPICS_CXX11_OVERRIDE override
#else
#  define EPICS_CXX11_FINAL
#  define EPICS_CXX11_OVERRIDE
#endif

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))

/*
 * Deprecation marker if possible
 */
#if  (__GNUC__ > 2)
#   define EPICS_DEPRECATED __attribute__((deprecated))
#endif

/*
 * Unused marker
 */
#define EPICS_UNUSED __attribute__((unused))

#endif  /* ifndef compilerSpecific_h */
