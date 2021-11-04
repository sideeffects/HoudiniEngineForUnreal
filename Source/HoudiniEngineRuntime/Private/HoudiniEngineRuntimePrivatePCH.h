/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Define module names.
#define HOUDINI_MODULE "HoudiniEngine"
#define HOUDINI_MODULE_EDITOR "HoudiniEngineEditor"
#define HOUDINI_MODULE_RUNTIME "HoudiniEngineRuntime"

// Declare the log category depending on the module we're in
#ifdef HOUDINI_ENGINE_EDITOR
	#define HOUDINI_LOCTEXT_NAMESPACE HOUDINI_MODULE_EDITOR
HOUDINIENGINEEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineEditor, Log, All);
#else
	#ifdef HOUDINI_ENGINE
		#define HOUDINI_LOCTEXT_NAMESPACE HOUDINI_MODULE
		HOUDINIENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
	#else
		#define HOUDINI_LOCTEXT_NAMESPACE HOUDINI_MODULE_RUNTIME
		HOUDINIENGINERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineRuntime, Log, All);
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------
// Default session settings
//---------------------------------------------------------------------------------------------------------------------

#define HAPI_UNREAL_SESSION_SERVER_AUTOSTART                true
#define HAPI_UNREAL_SESSION_SERVER_TIMEOUT                  3000.0f
#define HAPI_UNREAL_SESSION_SERVER_HOST                     TEXT( "localhost" )
#define HAPI_UNREAL_SESSION_SERVER_PORT                     9090
#if PLATFORM_MAC
	#define HAPI_UNREAL_SESSION_SERVER_PIPENAME                 TEXT( "/tmp/hapi" )
#else
	#define HAPI_UNREAL_SESSION_SERVER_PIPENAME                 TEXT( "hapi" )
#endif



// Names of HAPI libraries on different platforms.
#define HAPI_LIB_OBJECT_WINDOWS         TEXT( "libHAPIL.dll" )
#define HAPI_LIB_OBJECT_MAC             TEXT( "libHAPIL.dylib" )
#define HAPI_LIB_OBJECT_LINUX           TEXT( "libHAPIL.so" )

//---------------------------------------------------------------------------------------------------------------------
// LOG MACROS
//---------------------------------------------------------------------------------------------------------------------

// Whether to enable logging.
#define HOUDINI_ENGINE_LOGGING 1

#ifdef HOUDINI_ENGINE_LOGGING
	#ifdef HOUDINI_ENGINE
		#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...) \
				do \
				{ \
					UE_LOG( LogHoudiniEngine, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ); \
				} \
				while ( 0 )
	#else
		#ifdef HOUDINI_ENGINE_EDITOR
			#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...) \
				do \
				{ \
					UE_LOG( LogHoudiniEngineEditor, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ); \
				} \
				while ( 0 )
		#else
			#define HOUDINI_LOG_HELPER( VERBOSITY, HOUDINI_LOG_TEXT, ... ) \
				do \
				{ \
					UE_LOG( LogHoudiniEngineRuntime, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ); \
				} \
				while ( 0 )
		#endif
	#endif

	#define HOUDINI_LOG_MESSAGE( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_LOG_HELPER( Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__ )

	#define HOUDINI_LOG_FATAL( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_LOG_HELPER( Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__ )

	#define HOUDINI_LOG_ERROR( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_LOG_HELPER( Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__ )

	#define HOUDINI_LOG_WARNING( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_LOG_HELPER( Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__ )

	#define HOUDINI_LOG_DISPLAY( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_LOG_HELPER( Display, HOUDINI_LOG_TEXT, ##__VA_ARGS__ )
#else
	#define HOUDINI_LOG_MESSAGE( HOUDINI_LOG_TEXT, ... )
	#define HOUDINI_LOG_FATAL( HOUDINI_LOG_TEXT, ... )
	#define HOUDINI_LOG_ERROR( HOUDINI_LOG_TEXT, ... )
	#define HOUDINI_LOG_WARNING( HOUDINI_LOG_TEXT, ... )
	#define HOUDINI_LOG_DISPLAY( HOUDINI_LOG_TEXT, ... )
#endif


#define HOUDINI_DEBUG_EXPAND_UE_LOG(LONG_NAME, VERBOSITY, HOUDINI_LOG_TEXT, ...) \
	do \
	{ \
		UE_LOG( LogHoudiniEngine##LONG_NAME, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ); \
	} \
	while ( 0 )


// ---------------------------------------------------------
// Blueprint Debug Logging
// ---------------------------------------------------------
// Set HOUDINI_ENGINE_DEBUG_BP=1 to enable Blueprint logging
#if defined(HOUDINI_ENGINE_LOGGING) && defined(HOUDINI_ENGINE_DEBUG_BP)
	DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineBlueprint, Log, All);
	#define HOUDINI_BP_DEFINE_LOG_CATEGORY() \
			DEFINE_LOG_CATEGORY(LogHoudiniEngineBlueprint);
	#define HOUDINI_BP_MESSAGE( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Blueprint, Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BP_FATAL( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Blueprint, Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BP_ERROR( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Blueprint, Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BP_WARNING( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Blueprint, Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
#else 
	#define HOUDINI_BP_DEFINE_LOG_CATEGORY() 
	#define HOUDINI_BP_MESSAGE( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BP_FATAL( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BP_ERROR( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BP_WARNING( HOUDINI_LOG_TEXT, ... ) 
#endif


// ---------------------------------------------------------
// PDG Debug Logging
// ---------------------------------------------------------
// Set HOUDINI_ENGINE_DEBUG_PDG=1 to enable PDG logging
#if defined(HOUDINI_ENGINE_LOGGING) && defined(HOUDINI_ENGINE_DEBUG_PDG)
	DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEnginePDG, Log, All);
	#define HOUDINI_PDG_DEFINE_LOG_CATEGORY() \
			DEFINE_LOG_CATEGORY(LogHoudiniEnginePDG);
	#define HOUDINI_PDG_MESSAGE( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( PDG, Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_PDG_FATAL( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( PDG, Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_PDG_ERROR( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( PDG, Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_PDG_WARNING( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( PDG, Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
#else 
	#define HOUDINI_PDG_DEFINE_LOG_CATEGORY() 
	#define HOUDINI_PDG_MESSAGE( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_PDG_FATAL( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_PDG_ERROR( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_PDG_WARNING( HOUDINI_LOG_TEXT, ... ) 
#endif


// ---------------------------------------------------------
// Landscape Debug Logging
// ---------------------------------------------------------
// Set HOUDINI_ENGINE_DEBUG_LANDSCAPE=1 to enable PDG logging
#if defined(HOUDINI_ENGINE_LOGGING) && defined(HOUDINI_ENGINE_DEBUG_LANDSCAPE)
	DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineLandscape, Log, All);
	#define HOUDINI_LANDSCAPE_DEFINE_LOG_CATEGORY() \
			DEFINE_LOG_CATEGORY(LogHoudiniEngineLandscape);
	#define HOUDINI_LANDSCAPE_MESSAGE( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_LANDSCAPE_FATAL( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_LANDSCAPE_ERROR( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_LANDSCAPE_WARNING( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
#else 
	#define HOUDINI_LANDSCAPE_DEFINE_LOG_CATEGORY() 
	#define HOUDINI_LANDSCAPE_MESSAGE( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_LANDSCAPE_FATAL( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_LANDSCAPE_ERROR( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_LANDSCAPE_WARNING( HOUDINI_LOG_TEXT, ... ) 
#endif


// ---------------------------------------------------------
// Baking Debug Logging
// ---------------------------------------------------------
// Set HOUDINI_ENGINE_DEBUG_BAKING=1 to enable PDG logging
#if defined(HOUDINI_ENGINE_LOGGING) && defined(HOUDINI_ENGINE_DEBUG_BAKING)
	DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineBaking, Log, All);
	#define HOUDINI_BAKING_DEFINE_LOG_CATEGORY() \
			DEFINE_LOG_CATEGORY(LogHoudiniEngineBaking);
	#define HOUDINI_BAKING_MESSAGE( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BAKING_FATAL( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BAKING_ERROR( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
	#define HOUDINI_BAKING_WARNING( HOUDINI_LOG_TEXT, ... ) \
			HOUDINI_DEBUG_EXPAND_UE_LOG( Landscape, Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__ ) 
#else 
	#define HOUDINI_BAKING_DEFINE_LOG_CATEGORY() 
	#define HOUDINI_BAKING_MESSAGE( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BAKING_FATAL( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BAKING_ERROR( HOUDINI_LOG_TEXT, ... ) 
	#define HOUDINI_BAKING_WARNING( HOUDINI_LOG_TEXT, ... ) 
#endif


// HAPI_Common
enum HAPI_UNREAL_NodeType 
{
	HAPI_UNREAL_NODETYPE_ANY		=		-1,
	HAPU_UNREAL_NODETYPE_NONE		=		 0
};

enum HAPI_UNREAL_NodeFlags
{
	HAPI_UNREAL_NODEFLAGS_ANY = -1,
	HAPI_UNREAL_NODEFLAGS_NONE = 0
};

// Default cook/bake folder
#define HAPI_UNREAL_DEFAULT_BAKE_FOLDER					TEXT("/Game/HoudiniEngine/Bake");
#define HAPI_UNREAL_DEFAULT_TEMP_COOK_FOLDER			TEXT("/Game/HoudiniEngine/Temp");

// Various variable names used to store meta information in generated packages.
// More in HoudiniEnginePrivatePCH.h
#define HAPI_UNREAL_PACKAGE_META_TEMP_GUID				TEXT( "HoudiniPackageTempGUID" )
#define HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID			TEXT( "HoudiniComponentGUID" )

// Default PDG Filters
#define HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER				"HE_";
#define HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER		"HE_OUT_";

// Struct to enable global silent flag - this will force dialogs to not show up.
struct FHoudiniScopedGlobalSilence
{
	FHoudiniScopedGlobalSilence()
	{
		bGlobalSilent = GIsSilent;
		GIsSilent = true;
	}

	~FHoudiniScopedGlobalSilence()
	{
		GIsSilent = bGlobalSilent;
	}

	bool bGlobalSilent;
};

