/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
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
#include "Core.h"
#include "AssetData.h"
#include "MessageLog.h"
#include "UnrealNetwork.h"
#include "ComponentAssetBroker.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "PackageTools.h"
#include "ThumbnailHelpers.h"
#include "AssetRegistryModule.h"


/** Houdini Engine headers. **/
#include <vector>
#include <string>
#include "HAPI.h"

/** Other definitions. **/
#define HOUDINI_ENGINE_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
#define LOCTEXT_NAMESPACE "HoudiniEngine"

/** Some additional logging macros. **/
#ifdef HOUDINI_ENGINE_LOGGING

#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...)					\
	do																			\
	{																			\
		UE_LOG(LogHoudiniEngine, VERBOSITY, HOUDINI_LOG_TEXT, __VA_ARGS__);		\
	}																			\
	while(0)

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Log, HOUDINI_LOG_TEXT, __VA_ARGS__)	

#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Fatal, HOUDINI_LOG_TEXT, __VA_ARGS__)	

#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Error, HOUDINI_LOG_TEXT, __VA_ARGS__)	

#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Warning, HOUDINI_LOG_TEXT, __VA_ARGS__)	

#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Display, HOUDINI_LOG_TEXT, __VA_ARGS__)	

#elif

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT)
#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT)
#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT)
#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT)
#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT)

#endif // HOUDINI_ENGINE_LOGGING


/** Error checking - this macro will check the status and return specified parameter. **/
#define HOUDINI_CHECK_ERROR_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_RETURN, HAPI_LOG_ROUTINE)						\
	do																												\
	{																												\
		HAPI_Result Result = HAPI_PARAM_CALL;																		\
		if(HAPI_RESULT_SUCCESS != Result)																			\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
			return HAPI_PARAM_RETURN;																				\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR_RETURN(HAPI_PARAM_CALL, HAPI_PARAM_RETURN)												\
	HOUDINI_CHECK_ERROR_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_RETURN, HOUDINI_LOG_ERROR)


/** Error checking - this macro will check the status, execute specified parameter and return. **/
#define HOUDINI_CHECK_ERROR_EXECUTE_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN, HAPI_LOG_ROUTINE)		\
	do																												\
	{																												\
		HAPI_Result Result = HAPI_PARAM_CALL;																		\
		if(HAPI_RESULT_SUCCESS != Result)																			\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
			HAPI_PARAM_EXECUTE_RETURN;																				\
			return;																									\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR_EXECUTE_RETURN(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN)								\
	HOUDINI_CHECK_ERROR_EXECUTE_RETURN_HELPER(HAPI_PARAM_CALL, HAPI_PARAM_EXECUTE_RETURN, HOUDINI_LOG_ERROR)


/* Error checking - this macro will check the status. **/
#define HOUDINI_CHECK_ERROR_HELPER(HAPI_PARAM_RESULT, HAPI_PARAM_CALL, HAPI_LOG_ROUTINE)							\
	do																												\
	{																												\
		*HAPI_PARAM_RESULT = HAPI_PARAM_CALL;																		\
		if(HAPI_RESULT_SUCCESS != *HAPI_PARAM_RESULT)																\
		{																											\
			HAPI_LOG_ROUTINE(TEXT("Hapi failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());					\
		}																											\
	}																												\
	while(0)

#define HOUDINI_CHECK_ERROR(HAPI_PARAM_RESULT, HAPI_PARAM_CALL)														\
	HOUDINI_CHECK_ERROR_HELPER(HAPI_PARAM_RESULT, HAPI_PARAM_CALL, HOUDINI_LOG_ERROR)


/** HoudiniEngine Class headers. **/
#include "HoudiniMeshTriangle.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniAssetThumbnailRenderer.h"

/** HoudiniEngine Private headers. **/
#include "HoudiniEnginePrivatePatch.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniLogo.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniMeshIndexBuffer.h"
#include "HoudiniMeshVertexBuffer.h"
#include "HoudiniMeshVertexFactory.h"
#include "HoudiniMeshSceneProxy.h"
#include "HoudiniAssetThumbnailScene.h"
