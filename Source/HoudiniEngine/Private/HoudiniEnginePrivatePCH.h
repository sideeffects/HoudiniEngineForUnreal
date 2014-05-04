/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */
#pragma once

/** Unreal headers. **/
#include "CoreUObject.h"
#include "Engine.h"
#include "AssetTypeActions_Base.h"
#include "ModuleManager.h"
#include "UnrealEd.h"

/** HoudiniEngine Class headers. **/
#include "HoudiniEngineClasses.h"

/** HoudiniEngine Private headers. **/
#include "HoudiniEngine.h"
#include "HoudiniAssetTypeActions.h"

/** Other definitions. **/
#define HOUDINI_ENGINE_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
#define LOCTEXT_NAMESPACE "HoudiniEngine"

/** Some additional logging macros. **/
#ifdef HOUDINI_ENGINE_LOGGING

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT)									\
	do																			\
	{																			\
		UE_LOG(LogHoudiniEngine, Log, HOUDINI_LOG_TEXT);						\
	}																			\
	while(0)

#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT)									\
	do																			\
	{																			\
		UE_LOG(LogHoudiniEngine, Warning, HOUDINI_LOG_TEXT);					\
	}																			\
	while(0)


#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT)										\
	do																			\
	{																			\
		UE_LOG(LogHoudiniEngine, Error, HOUDINI_LOG_TEXT);						\
	}																			\
	while(0)

#endif
