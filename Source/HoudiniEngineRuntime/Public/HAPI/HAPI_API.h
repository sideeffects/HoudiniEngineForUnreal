/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * COMMENTS:
 */

#ifndef __HAPI_API_h__
#define __HAPI_API_h__

// Helper macros.
#if defined( __GNUC__ )
    #define HAPI_IS_GCC_GE( MAJOR, MINOR ) \
        ( __GNUC__ > MAJOR || (__GNUC__ == MAJOR && __GNUC_MINOR__ >= MINOR) )
#else
    #define HAPI_IS_GCC_GE( MAJOR, MINOR ) 0
#endif // defined( __GNUC__ )
#define HAPI_TO_STRING_( a ) # a
#define HAPI_TO_STRING( a ) HAPI_TO_STRING_( a )

// Mark function as deprecated and may be removed in the future.
// @note This qualifier can only appear in function declarations.
#ifdef HAPI_SUPPRESS_DEPRECATIONS
	#define HAPI_DEPRECATED( hapi_ver, houdini_ver )
	#define HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement )
#elif defined( GCC3 )
    #if HAPI_IS_GCC_GE( 4, 6 ) || defined( __clang__ )
        #define HAPI_DEPRECATED( hapi_ver, houdini_ver ) \
            __attribute__( ( deprecated( \
                "Deprecated since version HAPI " HAPI_TO_STRING( hapi_ver ) \
                ", Houdini " HAPI_TO_STRING( houdini_ver ) ) ) )
        #define HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement ) \
            __attribute__( ( deprecated( \
                "Deprecated since version HAPI " HAPI_TO_STRING( hapi_ver ) \
                ", Houdini "  HAPI_TO_STRING( houdini_ver ) ". Use " \
                HAPI_TO_STRING( replacement ) " instead." ) ) )
    #else
        #define HAPI_DEPRECATED( hapi_ver, houdini_ver ) \
            __attribute__( ( deprecated ) )
        #define HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement ) \
            __attribute__( ( deprecated ) )
    #endif
#elif defined( _MSC_VER )
    #define HAPI_DEPRECATED( hapi_ver, houdini_ver ) \
        __declspec( deprecated( \
            "Deprecated since version HAPI " HAPI_TO_STRING( hapi_ver ) \
            ", Houdini " HAPI_TO_STRING( houdini_ver ) ) )
    #define HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement ) \
        __declspec( deprecated( \
            "Deprecated since version HAPI " HAPI_TO_STRING( hapi_ver ) \
            ", Houdini "  HAPI_TO_STRING( houdini_ver ) ". Use " \
            HAPI_TO_STRING( replacement ) " instead." ) )
#else
    #define HAPI_DEPRECATED( hapi_ver, houdini_ver )
    #define HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement )
#endif

#if defined( WIN32 ) && !defined( MAKING_STATIC )
    #define HAPI_VISIBILITY_EXPORT __declspec( dllexport )
    #define HAPI_VISIBILITY_IMPORT __declspec( dllimport )
    #define HAPI_VISIBILITY_EXPORT_TINST HAPI_VISIBILITY_EXPORT
    #define HAPI_VISIBILITY_IMPORT_TINST HAPI_VISIBILITY_IMPORT
#elif defined(__GNUC__) && HAPI_IS_GCC_GE( 4, 2 ) && !defined( MAKING_STATIC )
    #define HAPI_VISIBILITY_EXPORT __attribute__( ( visibility( "default" ) ) )
    #define HAPI_VISIBILITY_IMPORT __attribute__( ( visibility( "default" ) ) )
    #define HAPI_VISIBILITY_EXPORT_TINST
    #define HAPI_VISIBILITY_IMPORT_TINST
#else
    #define HAPI_VISIBILITY_EXPORT
    #define HAPI_VISIBILITY_IMPORT
    #define HAPI_VISIBILITY_EXPORT_TINST
    #define HAPI_VISIBILITY_IMPORT_TINST
#endif

#ifdef HAPI_EXPORTS
    #define HAPI_API HAPI_VISIBILITY_EXPORT
#else
    #define HAPI_API HAPI_VISIBILITY_IMPORT
#endif // HAPI_EXPORTS

#if defined( WIN32 )
    #define HAPI_CALL __cdecl
#else
    #define HAPI_CALL
#endif // defined( WIN32 )

#ifdef __cplusplus
    #define HAPI_EXTERN_C extern "C"
#else
    #define HAPI_EXTERN_C
#endif // __cplusplus

#define HAPI_RETURN HAPI_Result HAPI_CALL

#define HAPI_DECL_RETURN( r ) HAPI_EXTERN_C HAPI_API r HAPI_CALL
#define HAPI_DECL_DEPRECATED( hapi_ver, houdini_ver ) \
    HAPI_EXTERN_C \
    HAPI_DEPRECATED( hapi_ver, houdini_ver ) \
    HAPI_API \
    HAPI_RETURN
#define HAPI_DECL_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement ) \
    HAPI_EXTERN_C \
    HAPI_DEPRECATED_REPLACE( hapi_ver, houdini_ver, replacement ) \
    HAPI_API \
    HAPI_RETURN

#define HAPI_DECL HAPI_EXTERN_C HAPI_API HAPI_RETURN
#define HAPI_DEF HAPI_EXTERN_C HAPI_RETURN

// Static asserts
#ifdef __cplusplus
#define HAPI_STATIC_ASSERT( cond, msg ) \
    template< bool b > \
    struct HAPI_StaticAssert_##msg {}; \
    template<> \
    struct HAPI_StaticAssert_##msg< true > \
    { \
        typedef int static_assert_##msg; \
    }; \
    typedef HAPI_StaticAssert_##msg< ( cond ) >::static_assert_##msg _sa
#else
    #define HAPI_STATIC_ASSERT( cond, msg ) HAPI_STATIC_ASSERT_I( cond, msg )
    #define HAPI_STATIC_ASSERT_I( cond, msg ) \
        typedef char static_assert_##msg[ 2 * ( cond ) - 1 ]
#endif // __cplusplus

#endif // __HAPI_API_h__
