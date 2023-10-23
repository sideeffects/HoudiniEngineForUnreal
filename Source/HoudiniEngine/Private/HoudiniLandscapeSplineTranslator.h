/*
* Copyright (c) <2023> Side Effects Software Inc.
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

// This file contains the "public" API for translating curves/splines from Houdini to Unreal Landscape Splines.

#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"

#include "HoudiniOutput.h"
#include "LandscapeSplineControlPoint.h"


// Unreal engine forward declarations
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
struct FLandscapeSplineSegmentConnection;

// Houdini Engine Plugin forward declarations
class UHoudiniAssetComponent;
struct FHoudiniPackageParams;
struct FLandscapeSplineInfo;
struct FLandscapeSplineCurveAttributes;
struct FLandscapeSplineSegmentMeshAttributes;
struct FHoudiniLandscapeSplineApplyLayerData;


struct HOUDINIENGINE_API FHoudiniLandscapeSplineTranslator
{
	/**
	 * @brief Process the landscape spline output InOutput and create/update the relevant ULandscapeSplinesComponents.
	 * @param InOutput A landscape output.
	 * @param InAllInputLandscaps Input landscapes. Used to find landscapes targets referenced as "Input#".
	 * @param InPackageParams Package parameters used as the basis for naming LandscapeSplineActors in world partition.
	 * @param ClearedLayers Map of landscape to a set of layer names: this map is checked and updated to keep track of
	 * which edit layers have been cleared during output processing.
	 * @return True if we successfully processed the output and create/update/removed the appropriate landscape splines.
	 */
	static bool ProcessLandscapeSplineOutput(
		UHoudiniOutput* const InOutput,
		const TArray<ALandscapeProxy*>& InAllInputLandscapes,
		UWorld* InWorld,
		const FHoudiniPackageParams& InPackageParams,
		TMap<ALandscape*, TSet<FName>>& ClearedLayers);

	/**
	 * @brief Create / update ULandscapeSplinesComponents from the geo in InHGPO.
	 * @param InHGPO The Houdini geo part object containing the curves to treat as landscape splines.
	 * @param InOutput The UHoudiniOutput instance of the InHGPO.
	 * @param InAllInputLandscaps Input landscapes. Used to find landscapes targets referenced as "Input#".
	 * @param InPackageParams Package parameters, used as the basis for naming LandscapeSplineActors in world partition
	 * and for temp edit layer names.
	 * @param InCurrentSplines The current landscape spline output objects (to be updated/replaced).
	 * @param bInForceRebuild If true then we always remove the existing landscape splines in InCurrentSplines and
	 * create new ones from the content of the InHGPO (instead of re-using existing splines).
	 * @param InFallbackLandscape If the InHGPO does not have the appropriate target landscape attribute, create the
	 * splines on this landscape.
	 * @param ClearedLayers Used to check if a layer has been cleared or already. Cleared layers are added.
	 * @param SegmentsToApplyToLayers Updated with per-landscape-layer segments that should be applied to the layer
	 * at the end of output processing.
	 * @param OutputSplines The created/updated landscape spline output objects.
	 * @param InHAC The HoudiniAssetComponent if available. Currently this is only to determine if temp edit layers
	 * should be created and for temp package naming.
	 * @return True if the landscape splines were successfully created/updated.
	 */
	static bool CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
		const FHoudiniGeoPartObject& InHGPO,
		UHoudiniOutput* InOutput,
		const TArray<ALandscapeProxy*>& InAllInputLandscapes,
		UWorld* InWorld,
		const FHoudiniPackageParams& InPackageParams,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InCurrentSplines,
		bool bInForceRebuild,
		ALandscapeProxy* InFallbackLandscape,
		TMap<ALandscape*, TSet<FName>>& ClearedLayers,
		TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputSplines,
		UHoudiniAssetComponent* InHAC=nullptr);

private:
	static void DeleteTempLandscapeLayers(UHoudiniOutput* InOutput);

	static void AddSegmentToOutputObject(
		ULandscapeSplineSegment* InSegment,
		const FLandscapeSplineCurveAttributes& InAttributes,
		int32 InVertexIndex,
		UHoudiniAssetComponent* InHAC,
		const FHoudiniPackageParams& InPackageParams,
		UHoudiniLandscapeSplinesOutput& InOutputObject);

	static void UpdateNonReservedEditLayers(
		const FLandscapeSplineInfo& InSplineInfo,
		TMap<ALandscape*, TSet<FName>>& ClearedLayers,
		TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers);

	static ULandscapeSplineControlPoint* GetOrCreateControlPoint(
		FLandscapeSplineInfo& SplineInfo, int32 InControlPointId, bool& bOutCreated);
	
	static bool CopySegmentMeshAttributesFromHoudini(
		HAPI_NodeId InNodeId,
		HAPI_PartId InPartId,
		HAPI_AttributeOwner InAttrOwner,
		int32 InStartIndex,
		int32 InCount,
		TArray<FLandscapeSplineSegmentMeshAttributes>& OutAttributes);

	static bool CopyCurveAttributesFromHoudini(
		HAPI_NodeId InNodeId,
		HAPI_PartId InPartId,
		int32 InPrimIndex,
		int32 InFirstPointIndex,
		int32 InNumPoints,
		FLandscapeSplineCurveAttributes& OutCurveAttributes);

	static bool UpdateControlPointFromAttributes(
		ULandscapeSplineControlPoint* InPoint,
		const FLandscapeSplineCurveAttributes& InAttributes,
		const FTransform& InTransformToApply,
		int32 InPointIndex);

	static bool UpdateSegmentFromAttributes(
		ULandscapeSplineSegment* InSegment, const FLandscapeSplineCurveAttributes& InAttributes, int32 InVertexIndex);
	
	static bool UpdateConnectionFromAttributes(
		FLandscapeSplineSegmentConnection& InConnection,
		const int32 InConnectionIndex,
		const FLandscapeSplineCurveAttributes& InAttributes,
		int32 InPointIndex);
};
