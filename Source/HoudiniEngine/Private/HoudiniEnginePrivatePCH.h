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
#include "Core.h"
#include "AssetData.h"
#include "MessageLog.h"
#include "UnrealNetwork.h"
#include "ComponentAssetBroker.h"
#include "LevelEditorViewport.h"

/** Houdini Engine headers. **/
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

/** HoudiniEngine Class headers. **/
#include "HoudiniMeshTriangle.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniAssetThumbnailRenderer.h"

/** HoudiniEngine Private headers. **/
#include "HoudiniEngine.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniAssetCooking.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniMeshIndexBuffer.h"
#include "HoudiniMeshVertexBuffer.h"
#include "HoudiniMeshVertexFactory.h"
#include "HoudiniMeshSceneProxy.h"
#include "HoudiniAssetComponentInstanceData.h"
