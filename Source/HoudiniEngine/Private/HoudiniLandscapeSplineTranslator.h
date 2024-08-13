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
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniPackageParams.h"
#include "HoudiniSplineTranslator.h"

#include "Landscape.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"

class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
struct FLandscapeSplineSegmentConnection;
class UHoudiniAssetComponent;
struct FHoudiniPackageParams;
struct FHoudiniLandscapeSplineInfo;
struct FHoudiniLandscapeSplineData;
struct FHoudiniLandscapeSplineSegmentMeshData;
struct FHoudiniLandscapeSplineApplyLayerData;




struct HOUDINIENGINE_API FHoudiniLandscapeSplineTranslator
{
	// Process the landscape spline output InOutput and create/update the relevant ULandscapeSplinesComponents.
	static bool ProcessLandscapeSplineOutput(
		UHoudiniOutput* InOutput,
		const TArray<ALandscapeProxy*>& InAllInputLandscapes,
		UWorld* InWorld,
		const FHoudiniPackageParams& InPackageParams,
		TMap<ALandscape*, TSet<FName>>& InClearedLayers);

	// Create / update ULandscapeSplinesComponents from the geo in InHGPO.
	static bool CreateOutputLandscapeSpline(
		const FHoudiniGeoPartObject& InHGPO,
		UHoudiniOutput* InOutput,
		const TArray<ALandscapeProxy*>& InAllInputLandscapes,
		UWorld* InWorld,
		const FHoudiniPackageParams& InPackageParams,
		ALandscapeProxy* InFallbackLandscape,
		TMap<ALandscape*, TSet<FName>>& InClearedLayers,
		TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects,
		UHoudiniAssetComponent* InHAC=nullptr);

private:
	static void DeleteTempLandscapeLayers(UHoudiniOutput* InOutput);

	static void AddSegmentToOutputObject(
		ULandscapeSplineSegment* InSegment,
		const FHoudiniLandscapeSplineData& InSplineData,
		int InVertexIndex,
		UHoudiniAssetComponent* InHAC,
		const FHoudiniPackageParams& InPackageParams,
		UHoudiniLandscapeSplinesOutput& InOutputObject);

	static void UpdateNonReservedEditLayers(
		const FHoudiniLandscapeSplineInfo& InSplineInfo,
		TMap<ALandscape*, TSet<FName>>& InClearedLayers,
		TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& InSegmentsToApplyToLayers);

	static ULandscapeSplineControlPoint* GetOrCreateControlPoint(
		FHoudiniLandscapeSplineInfo& SplineInfo, int InControlPointId, bool& bOutCreated);
	
	static bool CopySegmentMeshAttributesFromHoudini(
		HAPI_NodeId InNodeId,
		HAPI_PartId InPartId,
		HAPI_AttributeOwner InAttrOwner,
		int InStartIndex,
		int InCount,
		TArray<FHoudiniLandscapeSplineSegmentMeshData>& AllSegmentMeshData);

	static FHoudiniLandscapeSplineData GetSplineDataFromAttributes(
		HAPI_NodeId InNodeId,
		HAPI_PartId InPartId,
		int InPrimIndex,
		int InFirstPointIndex,
		int InNumPoints);

	static bool SetControlPointData(
		ULandscapeSplineControlPoint* InPoint,
		const FHoudiniLandscapeSplineData& SplineData,
		const FTransform& InTransformToApply,
		int InPointIndex);

	static bool SetSegmentData(ULandscapeSplineSegment* InSegment, const FHoudiniLandscapeSplineData& InSplineData, int InVertexIndex);
	
	static bool SetConnectionData(
		FLandscapeSplineSegmentConnection& InConnection,
		int InConnectionIndex,
		const FHoudiniLandscapeSplineData& InSplineData,
		int InPointIndex);

	static void GetCachedAttributes(FHoudiniOutputObject * OutputObject, const FHoudiniGeoPartObject& InHGPO, const FHoudiniLandscapeSplineInfo& SplineInfo);

	static FVector ConvertPositionToVector(const float* InPosition);

};

struct FHoudiniLandscapeSplineMesh
{
	FString MeshRef;
	TArray<FString> MaterialOverrideRef; // the outer index is material 0, 1, 2 ... 
	FVector MeshScale = FVector::OneVector;
	FVector2d CenterAdjust = FVector2d::Zero();
};


struct FHoudiniLandscapeSplineSegmentMeshData
{
	TArray<FHoudiniLandscapeSplineMesh> Meshes;
};


struct FHoudiniLandscapeSplineData
{
	//-------------------------------------------------------------------------------------------------------------------------------------
	// Attribute data associate with control points. This is the raw attribute data pulled from Houdini.
	// Since Houdini Engine doesn't really distinguish between curve vertices and points, this data can be store on either.
	//-------------------------------------------------------------------------------------------------------------------------------------

	TArray<float> PointPositions;
	TArray<float> PointRotations;
	TArray<FString> PointPaintLayerNames;
	TArray<int> PointRaiseTerrains;
	TArray<int> PointLowerTerrains;
	TArray<FString> PointMeshRefs;
	TArray<float> PointMeshScales;
	TArray<int> PointIds;
	TArray<float> PointHalfWidths;
	TArray<float> PointSideFalloffs;
	TArray<float> PointEndFalloffs;
	TArray<TArray<FString>> PerMaterialOverridePointRefs;

	//-------------------------------------------------------------------------------------------------------------------------------------
	// Attribute data associated with segments. This is the raw attribute data pulled from Houdini.
	// Since Houdini Engine doesn't really distinguish between curve vertices and points, this data can be store on either.
	//-------------------------------------------------------------------------------------------------------------------------------------

	TArray<FString> SegmentConnectionSocketNames[2]; // 0 - is the start, 1 - is the end.
	TArray<float> SegmentConnectionTangentLengths[2]; // 0 - is the start, 1 - is the end.
	TArray<FString> SegmentPaintLayerNames;
	TArray<int> SegmentRaiseTerrains;
	TArray<int> SegmentLowerTerrains;
	TArray<FString> SegmentEditLayers;
	TArray<int> SegmentEditLayersClear;
	TArray<FString> SegmentEditLayersAfter;
	TArray<FHoudiniLandscapeSplineSegmentMeshData> SegmentMeshData;

	//-------------------------------------------------------------------------------------------------------------------------------------
	// Default values for spline control points and segments. These are set to sensible defaults, but can be overriden
	// with primitive attributes
	//-------------------------------------------------------------------------------------------------------------------------------------

	float DefaultConnectionTangentLengths[2]; // near side (0) and far side (1) of the segment connection.
	FString DefaultPaintLayerName;
	int DefaultRaiseTerrain = 1;
	int DefaultLowerTerrain = 1;
	FString DefaultEditLayer;
	bool DefaultEditLayerClear = false;
	FString DefaultEditLayerAfter;
	TArray<FHoudiniLandscapeSplineSegmentMeshData> DefaultMeshSegmentData;

	/**
	* The mesh socket names on the splines' prims. The index is the near side (0) and far side (1) of the
	* segment connection.
	*/
	FString DefaultConnectionSocketNames[2];
};

// Transient/transactional struct for processing landscape spline output.
struct FHoudiniLandscapeSplineInfo
{
	// OutputObject Id.
	FHoudiniOutputObjectIdentifier Identifier;

	// Target output info.
	ALandscapeProxy* LandscapeProxy = nullptr;
	ALandscape* Landscape = nullptr;
	ULandscapeSplinesComponent* SplinesComponent = nullptr;


	// Package Params.
	FHoudiniPackageParams LayerPackageParams;
	FHoudiniPackageParams SplineActorPackageParams;

	// Data for World Partition only
	ALandscapeSplineActor* LandscapeSplineActor = nullptr;
	FName OutputName = NAME_None;

	 // Array of curve indices in the HGPO that will be used to create segments for this landscape spline. There can
	 // be more than one segment per curve.
	TArray<int> CurveIndices;

	 // An array per-curve that stores the index of the first point (corresponding to the P attribute) for the curve
	 // info in the HGPO.
	TArray<int> PerCurveFirstPointIndex;

	// An array per-curve that stores the number of points for the curve in the HGPO. 
	TArray<int> PerCurvePointCount;

	// Curve prim and point attributes read from Houdini to apply to ULandscapeSplineControlPoint/Segment. 
	TArray<FHoudiniLandscapeSplineData> SplineData;

	// Control points mapped by id that have been created for this splines component.
	TMap<int, ULandscapeSplineControlPoint*> ControlPointMap;

	// The next control point ID (largest ID seen + 1).
	int32 NextControlPointId = 0;

	// Object used to keep track of segments/control points that we create for the FHoudiniOutputObject.
	UHoudiniLandscapeSplinesOutput* SplinesOutputObject = nullptr;
};
