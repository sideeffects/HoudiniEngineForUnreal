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

// Unreal engine forward declarations
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
struct FLandscapeSplineSegmentConnection;

// Houdini Engine Plugin forward declarations
struct FLandscapeSplineInfo;
struct FLandscapeSplineCurveAttributes;
struct FLandscapeSplineSegmentMeshAttributes;


struct HOUDINIENGINE_API FHoudiniLandscapeSplineTranslator
{
	/**
	 * @brief Process the landscape spline output InOutput and create/update the relevant ULandscapeSplinesComponents.
	 * @param InOutput A landscape output.
	 * @param InOuterComponent The owner, likely a UHoudiniAssetComponent, of InOutput.
	 * @return True if we successfully processed the output and create/update/removed the appropriate landscape splines.
	 */
	static bool ProcessLandscapeSplineOutput(UHoudiniOutput* const InOutput, UObject* const InOuterComponent);

	/**
	 * @brief Create / update ULandscapeSplinesComponents from the geo in InHGPO.
	 * @param InHGPO The Houdini geo part object containing the curves to treat as landscape splines.
	 * @param InOuterComponent The outer component, likely a HoudiniAssetComponent.
	 * @param InCurrentSplines The current landscape spline output objects (to be updated/replaced).
	 * @param bInForceRebuild If true then we always removed the existing landscape splines in InCurrentSplines and
	 * create new ones from the content of the InHGPO (instead of re-using existing splines).
	 * @param InFallbackLandscape If the InHGPO does not have the appropriate target landscape attribute, create the
	 * splines on this landscape.
	 * @param OutputSplines The created/updated landscape spline output objects.
	 * @return True if the landscape splines were successfully created/updated.
	 */
	static bool CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
		const FHoudiniGeoPartObject& InHGPO,
		UObject* InOuterComponent,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InCurrentSplines,
		bool bInForceRebuild,
		ALandscapeProxy* InFallbackLandscape,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputSplines);

private:
	static bool DestroyLandscapeSplinesSegmentsAndControlPoints(ULandscapeSplinesComponent* InSplinesComponent);

	static ULandscapeSplineControlPoint* GetOrCreateControlPoint(
		FLandscapeSplineInfo& SplineInfo, FName InDesiredName, bool& bOutCreated);
	
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
		const FTransform& InWorldTransform,
		int32 InPointIndex);

	static bool UpdateSegmentFromAttributes(
		ULandscapeSplineSegment* InSegment, const FLandscapeSplineCurveAttributes& InAttributes, int32 InVertexIndex);
	
	static bool UpdateConnectionFromAttributes(
		FLandscapeSplineSegmentConnection& InConnection,
		const int32 InConnectionIndex,
		const FLandscapeSplineCurveAttributes& InAttributes,
		int32 InPointIndex);
};
