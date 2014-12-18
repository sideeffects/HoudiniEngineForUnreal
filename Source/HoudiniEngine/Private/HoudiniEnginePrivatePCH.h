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
#include "UnrealEd.h"
#include "ObjectTools.h"
#include "AssetTypeActions_Base.h"
#include "ModuleManager.h"
#include "EngineModule.h"
#include "Core.h"
#include "AssetData.h"
#include "ComponentAssetBroker.h"
#include "PackageTools.h"
#include "ThumbnailHelpers.h"
#include "AssetRegistryModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "LevelEditor.h"
#include "IMainFrameModule.h"
#include "DesktopPlatformModule.h"
#include "RawMesh.h"
#include "StaticMeshResources.h"
#include "ComponentReregisterContext.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "SAssetDropTarget.h"
#include "SAssetSearchBox.h"
#include "SColorBlock.h"
#include "SColorPicker.h"
#include "SNumericEntryBox.h"
#include "SRotatorInputBox.h"
#include "SVectorInputBox.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Private/GeomFitUtils.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"

/** Houdini Engine headers. **/
#include <vector>
#include <string>
#include <stdint.h>
#include "HAPI.h"
#include "HAPI_Version.h"

/** Other definitions. **/
#define HOUDINI_ENGINE_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
#define LOCTEXT_NAMESPACE "HoudiniEngine"

/** Some additional logging macros. **/
#ifdef HOUDINI_ENGINE_LOGGING

#define HOUDINI_LOG_HELPER(VERBOSITY, HOUDINI_LOG_TEXT, ...)					\
	do																			\
	{																			\
		UE_LOG(LogHoudiniEngine, VERBOSITY, HOUDINI_LOG_TEXT, ##__VA_ARGS__);   \
	}																			\
	while(0)

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Log, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Fatal, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Error, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Warning, HOUDINI_LOG_TEXT, ##__VA_ARGS__)

#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT, ...)								\
	HOUDINI_LOG_HELPER(Display, HOUDINI_LOG_TEXT, ##__VA_ARGS__)	

#else

#define HOUDINI_LOG_MESSAGE(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_FATAL(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_ERROR(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_WARNING(HOUDINI_LOG_TEXT, ...)
#define HOUDINI_LOG_DISPLAY(HOUDINI_LOG_TEXT, ...)

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

/** HAPI related attribute definitions. **/
#define HAPI_UNREAL_ATTRIB_TANGENT						"unreal_tangent"
#define HAPI_UNREAL_ATTRIB_BINORMAL						"unreal_binormal"
#define HAPI_UNREAL_ATTRIB_MATERIAL						"unreal_face_material"
#define HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK			"unreal_face_smoothing_mask"
#define HAPI_UNREAL_ATTRIB_INSTANCE						"instance"
#define HAPI_UNREAL_ATTRIB_INSTANCE_ROTATION			"rot"
#define HAPI_UNREAL_ATTRIB_INSTANCE_SCALE				"scale"
#define HAPI_UNREAL_ATTRIB_INSTANCE_POSITION			"P"
#define HAPI_UNREAL_ATTRIB_POSITION						"P"
#define HAPI_UNREAL_PARAM_CURVE_TYPE					"type"
#define HAPI_UNREAL_PARAM_CURVE_METHOD					"method"
#define HAPI_UNREAL_PARAM_CURVE_COORDS					"coords"
#define HAPI_UNREAL_PARAM_CURVE_CLOSED					"close"

#define HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT	"0.0, 0.0, 3.0 3.0, 0.0, 3.0"

/** Suffix for all Unreal materials which are generated from Houdini. **/
#define HAPI_UNREAL_GENERATED_MATERIAL_SUFFIX			TEXT("_houdini_material")

/** Helper function to serialize enumerations. **/
template <typename TEnum>
FORCEINLINE
FArchive&
SerializeEnumeration(FArchive& Ar, TEnum& E)
{
	uint8 B = (uint8) E;
	Ar << B;

	if(Ar.IsLoading())
	{
		E = (TEnum) B;
	}

	return Ar;
}


/** HoudiniEngine Special headers. **/
#include "HoudiniGeoPartObject.h"

/** HoudiniEngine Class headers. **/
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniAssetThumbnailRenderer.h"

/** HoudiniEngine Private headers. **/
#include "HoudiniApi.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniLogo.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetThumbnailScene.h"
