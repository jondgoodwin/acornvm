/** Provides architecture-specific definitions
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_env_h
#define avm_env_h

// A convenience macro for assert(), establishing the conditions expected to be true, before returning expression e
#define assert_exp(c,e)	(assert(c), (e))

/* An imperfect way to define a float whose size is identical to ptrdiff_t. */
#if _WIN32 || _WIN64
	#if _WIN64
		#define AVM_ARCH 64
		#define ptrdiff_float double
	#else
		#define AVM_ARCH 32
		#define ptrdiff_float float
	#endif
#endif
#if __GNUC__
	#if __x86_64__ || __ppc64__
		#define AVM_ARCH 64
		#define ptrdiff_float double
	#else
		#define AVM_ARCH 32
		#define ptrdiff_float float
	#endif
#endif

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined( __BCPLUSPLUS__)  || defined( __MWERKS__)
    #if defined( AVM_LIBRARY_STATIC )
		#define AVM_API
    #elif defined( AVM_LIBRARY )
		#define AVM_API   extern __declspec(dllexport)
    #else
		#define AVM_API   extern __declspec(dllimport)
    #endif
#else
    #define AVM_API
#endif
#if defined(__cplusplus) && !defined(_MSC_VER)
	#define AVM_INLINE static inline
#else
	#define AVM_INLINE
#endif

// disable VisualStudio warnings
#if defined(_MSC_VER) && defined(OSG_DISABLE_MSVC_WARNINGS)
    #pragma warning( disable : 4244 )
    #pragma warning( disable : 4251 )
    #pragma warning( disable : 4275 )
    #pragma warning( disable : 4512 )
    #pragma warning( disable : 4267 )
    #pragma warning( disable : 4702 )
    #pragma warning( disable : 4511 )
#endif


#endif