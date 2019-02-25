
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXT_FCONTEXT_H
#define BOOST_CONTEXT_FCONTEXT_H

#if defined(__PGI)
#include <stdint.h>
#endif

#if defined(_WIN32_WCE)
typedef int intptr_t;
#endif

#include <cstddef>

#ifdef WIN32
#define CONTEXT_WINDOWS 1
#endif

#undef CONTEXT_CALLDECL
#if (defined(i386) || defined(__i386__) || defined(__i386) \
     || defined(__i486__) || defined(__i586__) || defined(__i686__) \
     || defined(__X86__) || defined(_X86_) || defined(__THW_INTEL__) \
     || defined(__I86__) || defined(__INTEL__) || defined(__IA32__) \
     || defined(_M_IX86) || defined(_I86_)) && defined(CONTEXT_WINDOWS)
# define CONTEXT_CALLDECL __cdecl
#else
# define CONTEXT_CALLDECL
#endif

typedef void*   fcontext_t;
extern "C" intptr_t CONTEXT_CALLDECL jump_fcontext( fcontext_t * ofc, fcontext_t nfc, intptr_t vp, bool preserve_fpu = false);
extern "C" fcontext_t CONTEXT_CALLDECL make_fcontext( void * sp, std::size_t size, void (* fn)( intptr_t) );

#endif // BOOST_CONTEXT_FCONTEXT_H

