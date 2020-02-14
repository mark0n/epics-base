#ifndef cpp11_h
#define cpp11_h
 
#ifndef __clang__
#   error compiler/clang/compilerSpecific.h is only for use with the clang compiler
#endif

// https://en.cppreference.com/w/cpp/language/noexcept
#if !__has_feature(cxx_noexcept)
#  define noexcept
#endif

// https://en.cppreference.com/w/cpp/language/final
// https://en.cppreference.com/w/cpp/language/override
#if !__has_feature(cxx_override_control)
#  define final
#  define override
#endif

#endif /* cpp11_h */
