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
#include "HoudiniEngineRuntime.h"

class UHoudiniOutput;
class UHoudiniInput;
class AActor;
class UActorComponent;
class USceneComponent;
class UHoudiniCookable;

struct FHoudiniObjectInfo;
struct FHoudiniGeoInfo;
struct FHoudiniPartInfo;
struct FHoudiniVolumeInfo;
struct FHoudiniCurveInfo;
struct FHoudiniPackageParams;
struct FHoudiniStaticMeshGenerationProperties;
struct FMeshBuildSettings;

enum class EHoudiniOutputType : uint8;
enum class EHoudiniGeoType : uint8;
enum class EHoudiniPartType : uint8;
enum class EHoudiniCurveType : int8;

struct HOUDINIENGINE_API FHoudiniOutputTranslator
{
public:

	// 
	static bool UpdateOutputs(UHoudiniCookable* HC);

	//
	static bool ProcessOutputs(
		UHoudiniCookable* HC,
		bool& bOutHasHoudiniStaticMeshOutput);

	//
	static bool BuildStaticMeshesOnHoudiniProxyMeshOutputs(
		UHoudiniCookable* HC,
		bool bInDestroyProxies = false);

	//
	static bool UpdateLoadedOutputs(
		HAPI_NodeId InNodeId,
		TArray<TObjectPtr<UHoudiniOutput>>& InOutputs,
		USceneComponent* InComponent);

	//
	static bool UploadChangedEditableOutput(
		TArray<TObjectPtr<UHoudiniOutput>>& InOutputs);
	//
	static bool BuildAllOutputs(
		HAPI_NodeId AssetId,
		UObject* InOuterObject,
		const TArray<HAPI_NodeId>& OutputNodes,
		const TMap<HAPI_NodeId, int32>& OutputNodeCookCounts,
		TArray<TObjectPtr<UHoudiniOutput>>& InOldOutputs,
		TArray<TObjectPtr<UHoudiniOutput>>& OutNewOutputs,
		bool InOutputTemplatedGeos,
		bool InUseOutputNodes,
		bool bGatherEditableCurves,
		bool bCreateSceneComponents);

	// Helpers functions used to convert HAPI types
	static EHoudiniGeoType ConvertHapiGeoType(const HAPI_GeoType& InType);
	static EHoudiniPartType ConvertHapiPartType(const HAPI_PartType& InType);
	static EHoudiniCurveType ConvertHapiCurveType(const HAPI_CurveType& InType);

	// Helper functions used to cache HAPI infos
	static void CacheObjectInfo(const HAPI_ObjectInfo& InObjInfo, FHoudiniObjectInfo& OutObjInfoCache);
	static void CacheGeoInfo(const HAPI_GeoInfo& InGeoInfo, FHoudiniGeoInfo& OutGeoInfoCache);
	static void CachePartInfo(const HAPI_PartInfo& InPartInfo, FHoudiniPartInfo& OutPartInfoCache);
	static void CacheVolumeInfo(const HAPI_VolumeInfo& InVolumeInfo, FHoudiniVolumeInfo& OutVolumeInfoCache);
	static void CacheCurveInfo(const HAPI_CurveInfo& InCurveInfo, FHoudiniCurveInfo& OutCurveInfoCache);

	// Helper to clear all outputs
	static void ClearAndRemoveOutputs(TArray<TObjectPtr<UHoudiniOutput>>& OutputsToClear, EHoudiniClearFlags ClearFlags);

	// Helper to clear an individual UHoudiniOutput
	static void ClearOutput(UHoudiniOutput* Output);

	static bool GetCustomPartNameFromAttribute(HAPI_NodeId NodeId, HAPI_PartId PartId, FString& OutCustomPartName);

protected:

	// 1. Update the output objects
	static void UpdateOutputObjects(
		HAPI_NodeId InNodeId,
		TArray<TObjectPtr<UHoudiniOutput>>& Outputs,
		const TArray<int32>& InNodeIdsToCook,
		const TMap<int32, int32>& InOutputNodeCookCounts,
		UObject* InOuter,
		bool bOutputless,
		bool bOutputTemplateGeos,
		bool bUseOutputNodes,
		bool bEnableCurveEditing,
		bool bCreateComponents);

	// 2. Update tags and generic attributes on HAC
	static bool UpdateOutputAttributesAndTags(UHoudiniCookable* InHC);

	// 3. Create the actual outputs assets/components
	static bool CreateAllOutputs(
		TArray<TObjectPtr<UHoudiniOutput>>& Outputs,
		const TArray<TObjectPtr<UHoudiniInput>>& Inputs,
		const FHoudiniPackageParams& PackageParams,
		UObject* InOuterComponent,
		UWorld* InWorld,
		bool bIsProxyStaticMeshEnabled,
		bool bHasNoProxyMeshNextCookBeenRequested,
		bool bIsBakeAfterNextCookEnabled,
		bool bSplitMeshSupport,
		const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties,
		const FMeshBuildSettings& InStaticMeshBuildSettings,
		bool& bOutHasHoudiniStaticMeshOutput,
		TArray<UPackage*>& OutCreatedPackages);

	// 4. Output cleanup
	static void CleanOutputsPostCreate(
		TArray<TObjectPtr<UHoudiniOutput>>& Outputs,
		UWorld* InWorld,
		bool bHasBeenLoaded);

	// 5. Update Data Layers and level instances
	static void UpdateDataLayersAndLevelInstanceOnOutput(
		TArray<TObjectPtr<UHoudiniOutput>>& InOutputs);

};