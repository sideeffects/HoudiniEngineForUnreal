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

#include "HAPI/HAPI_Common.h"
#include "CoreMinimal.h"

#include "HoudiniEnginePrivatePCH.h"

class UHoudiniAsset;
class UHoudiniCookable;
class UHoudiniParameter;
class UHoudiniParameterFile;

enum class EHoudiniFolderParameterType : uint8;
enum class EHoudiniParameterType : uint8;

struct HOUDINIENGINE_API FHoudiniParameterTranslator
{
	static bool UpdateParameters(
		HAPI_NodeId InNodeId,
		UObject* InOuter,
		TArray<TObjectPtr<UHoudiniParameter>>& InParameters,
		UHoudiniAsset* InHoudiniAsset,
		const FString& InHapiAssetName,
		bool bForceFullUpdate,
		bool bCacheRampParms,
		bool& bNeedToUpdateEditorProperties);

	static bool OnPreCookParameters(TArray<TObjectPtr<UHoudiniParameter>>& InParams);

	//
	static bool UpdateLoadedParameters(
		HAPI_NodeId InNodeId,
		TArray<TObjectPtr<UHoudiniParameter>>& InParameters,
		UObject* InOuter,
		bool bForceFullUpdate,
		bool bCacheRampParams,
		bool& bNeedToUpdateEditorProperties);

	// 
	static bool UploadChangedParameters(
		TArray<TObjectPtr<UHoudiniParameter>>& InParameters,
		HAPI_NodeId InNodeId);

	//
	static bool UploadParameterValue(UHoudiniParameter* InParam);

	//
	static bool UploadMultiParmValues(UHoudiniParameter* InParam);

	//
	static bool UploadRampParameter(UHoudiniParameter* InParam);

	//
	static bool UploadDirectoryPath(UHoudiniParameterFile* InParam);

	//
	static bool RevertParameterToDefault(UHoudiniParameter* InParam);

	//
	static bool SyncMultiParmValuesAtLoad(
		UHoudiniParameter* MultiParam, TArray<TObjectPtr<UHoudiniParameter>> &OldParams, const int32& InAssetId, const HAPI_AssetInfo& AssetInfo);
	
	// 
	static bool GetMultiParmInstanceStartIdx(
		const HAPI_AssetInfo& InAssetInfo,
		const FString InParmName, 
		int32& OutStartIdx,
		int32& OutInstanceCount,
		HAPI_ParmId& OutParmId,
		TArray<HAPI_ParmInfo> &OutParmInfos);

	/** Update parameters from the asset, re-uses parameters passed into CurrentParameters.
	@AssetId: Id of the digital asset
	@PrimaryObject: Object to use for transactions and as Outer for new top-level parameters
	@CurrentParameters: pre: current & post: invalid parameters
	@NewParameters: new params added to this

	On Return: CurrentParameters are the old parameters that are no longer valid,
		NewParameters are new and re-used parameters.
	*/
	static bool BuildAllParameters(
		HAPI_NodeId AssetId,
		class UObject* OuterObject,
		TArray<TObjectPtr<UHoudiniParameter>>& CurrentParameters,
		TArray<TObjectPtr<UHoudiniParameter>>& NewParameters,
		bool bUpdateValues,
		bool InForceFullUpdate,
		const UHoudiniAsset* InHoudiniAsset,
		const FString& InHoudiniAssetName,
		bool bCacheRampParms);

	// Parameter creation
	static UHoudiniParameter * CreateTypedParameter(
		class UObject * Outer,
		const EHoudiniParameterType& ParmType,
		const FString& ParmName );

	// Parameter update
	// bFullUpdate should be set to false after a minor update (change/recook) of the parameter
	// and set to true when creating a new parameter
	// bUpdateValue should be set to false when updating loaded parameters
	// as the internal parameter's value from HAPI
	static bool UpdateParameterFromInfo(
		UHoudiniParameter * HoudiniParameter,
		HAPI_NodeId InNodeId,
		const HAPI_ParmInfo& ParmInfo,
		bool bFullUpdate = true,
		bool bUpdateValue = true,
		const TArray<int>* DefaultIntValues = nullptr,
		const TArray<float>* DefaultFloatValues = nullptr,
		const TArray<HAPI_StringHandle>* DefaultStringValues = nullptr,
		const TArray<HAPI_ParmChoiceInfo>* DefaultChoiceValues = nullptr);

	static UClass* GetDesiredParameterClass(const HAPI_ParmInfo& ParmInfo);

	static void GetParmTypeFromParmInfo(
		const HAPI_ParmInfo& ParmInfo,
		EHoudiniParameterType& ParmType);

	static bool CheckParameterTypeAndClassMatch(
		UHoudiniParameter* Parameter,
		const EHoudiniParameterType& ParmType);
	
	/*
	static bool CheckParameterClassAndInfoMatch(
		UHoudiniParameter* Parameter, 
		const HAPI_ParmInfo& ParmInfo );
	*/

	// HAPI: Get a parameter's tag value.
	static bool HapiGetParameterTagValue(
		HAPI_NodeId NodeId,
		HAPI_ParmId ParmId,
		const FString& Tag,
		FString& TagValue);

	// HAPI: Get a parameter's unit.
	static bool HapiGetParameterUnit(
		HAPI_NodeId NodeId,
		HAPI_ParmId ParmId,
		FString& OutUnitString );

	// HAPI: Indicates if a parameter has a given tag
	static bool HapiGetParameterHasTag(
		HAPI_NodeId NodeId,
		HAPI_ParmId ParmId,
		const FString& Tag);

	// Get folder parameter type from HAPI_ParmInfo struct
	static EHoudiniFolderParameterType GetFolderTypeFromParamInfo(
		const HAPI_ParmInfo* ParamInfo);

	static bool RevertRampParameters(TMap<FString, UHoudiniParameter*> & InRampParams, const int32 & AssetId);
};