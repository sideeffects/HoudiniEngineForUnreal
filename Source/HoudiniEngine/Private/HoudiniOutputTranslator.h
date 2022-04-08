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

#include "CoreMinimal.h"

class UHoudiniOutput;
class UHoudiniAssetComponent;

struct FHoudiniObjectInfo;
struct FHoudiniGeoInfo;
struct FHoudiniPartInfo;
struct FHoudiniVolumeInfo;
struct FHoudiniCurveInfo;

enum class EHoudiniOutputType : uint8;
enum class EHoudiniGeoType : uint8;
enum class EHoudiniPartType : uint8;
enum class EHoudiniCurveType : int8;

struct HOUDINIENGINE_API FHoudiniOutputTranslator
{
	// 
	static bool UpdateOutputs(
		UHoudiniAssetComponent* HAC,
		const bool& bInForceUpdate,
		bool& bOutHasHoudiniStaticMeshOutput);

	//
	static bool BuildStaticMeshesOnHoudiniProxyMeshOutputs(UHoudiniAssetComponent* HAC, bool bInDestroyProxies=false);

	//
	static bool UpdateLoadedOutputs(UHoudiniAssetComponent* HAC);

	//
	static bool UploadChangedEditableOutput(
		UHoudiniAssetComponent* HAC,
		const bool& bInForceUpdate);
	//
	static bool BuildAllOutputs(
		const HAPI_NodeId& AssetId,
		UObject* InOuterObject,
		const TArray<HAPI_NodeId>& OutputNodes,
		const TMap<HAPI_NodeId, int32>& OutputNodeCookCounts,
		TArray<UHoudiniOutput*>& InOldOutputs,
		TArray<UHoudiniOutput*>& OutNewOutputs,
		const bool& InOutputTemplatedGeos,
		const bool& InUseOutputNodes);

	static bool UpdateChangedOutputs(
		UHoudiniAssetComponent* HAC);

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

	/** 
	 * Helper to clear the outputs of the houdini asset component
	 *
	 * Some outputs (such as landscapes) need "deferred clearing". This means that
	 * these outputs should only be destroyed AFTER the new outputs have been processed.
	 * 
	 * @param   InHAC	All outputs for this Houdini Asset Component will be cleared. 
	 * @param   OutputsPendingClear	Any outputs that is "pending" clear. These outputs should typically be cleared AFTER the new outputs have been fully processed.
	 * @param   bForceClearAll	Setting this flag will force outputs to be cleared here and not take into account outputs requested a deferred clear.
	 */
	static void ClearAndRemoveOutputs(UHoudiniAssetComponent *InHAC, TArray<UHoudiniOutput*>& OutputsPendingClear, bool bForceClearAll = false);
	// Helper to clear an individual UHoudiniOutput
	static void ClearOutput(UHoudiniOutput* Output);

	static bool GetCustomPartNameFromAttribute(const HAPI_NodeId & NodeId, const HAPI_PartId & PartId, FString & OutCustomPartName);
};