#ifndef cpp11_h
#define cpp11_h
 
#ifndef __GNUC__
#   error compiler/gcc/compilerSpecific.h is only for use with the gnu compiler
#endif

#ifdef __clang__
#   error compiler/gcc/compilerSpecific.h is not for use with the clang compiler
#endif

// https://en.cppreference.com/w/cpp/language/noexcept
#if (EPICS_GCC_VERSION < 40600) || !defined(EPICS_GCC_CXX11)
#  define noexcept
#endif

// https://en.cppreference.com/w/cpp/language/final
// https://en.cppreference.com/w/cpp/language/override
#if (EPICS_GCC_VERSION < 40700) || !defined(EPICS_GCC_CXX11)
#  define final
#  define override
#endif

#endif /* cpp11_h */
