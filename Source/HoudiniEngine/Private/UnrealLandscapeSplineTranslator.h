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

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "LandscapeSplinesComponent.h"

#include "HAPI/HAPI_Common.h"

struct FHoudiniUnrealLandscapeSplinesData;
class FHoudiniUnrealLandscapeSplineControlPointAttributes;
struct FLandscapeSplineInterpPoint;
class ALandscapeProxy;
class ULandscapeSplinesComponent;
class UMaterialInterface;
class UStaticMesh;

class FUnrealObjectInputHandle;
struct FHoudiniUnrealLandscapeSplineSegmentMeshData;
struct FHoudiniUnrealLandscapeSplinesControlPointData;
struct FHoudiniUnrealLandscapeSplinesData;


enum class EHoudiniUnrealLandscapeSplineCurve 
{
	Center = 0,
	Left = 1,
	Right = 2
};


class FHoudiniUnrealLandscapeSplineControlPointAttributes
{
public:
	/** Empties all arrays and reserve enough space for InExpectedPointCount entries. */
	void Init(int32 InPointCount);

	/** Add an entry to each array with the property values from InControlPoint. */
	bool AddControlPointData(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		TObjectPtr<ULandscapeSplineControlPoint>& InControlPoint,
#else
		const ULandscapeSplineControlPoint* InControlPoint,
#endif
		int32 InControlPointIndex,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId);

	/** Add an empty / default initialized entry to each array. */
	void AddEmpty();

	TArray<float> Rotations;
	TArray<FString> PaintLayerNames;
	TArray<int8> RaiseTerrains;
	TArray<int8> LowerTerrains;
	TArray<FString> MeshRefs;
	TArray<TArray<FString>> MaterialOverrideRefs;
	TArray<float> MeshScales;
	TArray<int32> Ids;
	TArray<float> HalfWidths;
	TArray<float> SideFalloffs;
	TArray<float> EndFalloffs;

private:
	int32 PointCount = 0;
};


struct FHoudiniUnrealLandscapeSplineSegmentMeshData
{
	TArray<FString> MeshRefs;
	TArray<TArray<FString>> MeshMaterialOverrideRefs;
	TArray<float> MeshScales;
};


struct FHoudiniUnrealLandscapeSplinesData
{
	/** Point positions (xyz) for all segments. */
	TArray<float> PointPositions;

	/** Vertex counts: the number of vertices per landscape spline. */
	TArray<int32> VertexCounts;

	/** Per-segment paint layer names */
	TArray<FString> SegmentPaintLayerNames;

	/** Per-segment bRaiseTerrain */
	TArray<int8> SegmentRaiseTerrains;

	/** Per-segment bLowerTerrain */
	TArray<int8> SegmentLowerTerrains;

	/** Static mesh attribute, the outer index is mesh 0, 1, 2 ... The struct contains the per-segment data */
	TArray<FHoudiniUnrealLandscapeSplineSegmentMeshData> PerMeshSegmentData;

	/**
	 * The mesh socket names on the splines' points, each index is a point index. Only the point indices that
	 * correspond to control points (first and last point of each segment) will have values set, the rest of the
	 * array will contain empty strings.
	 */
	TArray<FString> PointConnectionSocketNames;

	/**
	 * If a point corresponds with a control point on the spline, this contains the control point's tangent length
	 * for the segment connection.
	 */
	TArray<float> PointConnectionTangentLengths;

	/** Control point specific attributes. */
	FHoudiniUnrealLandscapeSplineControlPointAttributes ControlPointAttributes;
};


struct FHoudiniUnrealLandscapeSplinesControlPointData
{
	/**
	 * The control point positions of the splines. These are the original positions unaffected by connection mesh
	 * sockets.
	 */
	TArray<float> ControlPointPositions;

	/** Control point attributes. */
	FHoudiniUnrealLandscapeSplineControlPointAttributes Attributes;
};


/**
 * Helper struct for storing unresampled points (center, left and right) and the point's normalized [0, 1] position
 * along the spline.
 */
class FHoudiniUnResampledPoint
{
public:
	FHoudiniUnResampledPoint() = delete;

	FHoudiniUnResampledPoint(EHoudiniUnrealLandscapeSplineCurve InSplineSelection);
	FHoudiniUnResampledPoint(EHoudiniUnrealLandscapeSplineCurve InSplineSelection, const FLandscapeSplineInterpPoint& InPoint);

	FVector GetSelectedPosition() const;
	FQuat CalculateRotationTo(const FHoudiniUnResampledPoint& InNextPoint);

	FVector Center;
	FVector Left;
	FVector Right;
	FVector FalloffLeft;
	FVector FalloffRight;
	FQuat Rotation;
	float Alpha;
	EHoudiniUnrealLandscapeSplineCurve SplineSelection;
};

struct HOUDINIENGINE_API FUnrealLandscapeSplineTranslator 
{
public:
	// Create HAPI nodes and send the landscape splines of InLandscapeSplines to Houdini.
	static bool CreateInputNode(
		ULandscapeSplinesComponent* const InSplinesComponent,
		bool bForceReferenceInputNodeCreation,
		HAPI_NodeId& OutInputNodeId,
		FUnrealObjectInputHandle& OutInputNodeHandle,
		const FString& InNodeName,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		float InSplineResolution=0.0f,
		bool bInExportCurves=true,
		bool bInExportControlPoints=false,
		bool bInExportLeftRightCurves=false,
		bool bInInputNodesCanBeDeleted=true);

	// Create a null SOP with a curve for each spline/segment of InSplinesComponent.
	static bool CreateInputNode(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		HAPI_NodeId InObjectNodeId,
		const FString& InNodeName,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		HAPI_NodeId& OutNodeId,
		EHoudiniUnrealLandscapeSplineCurve InExportCurve=EHoudiniUnrealLandscapeSplineCurve::Center,
		float InSplineResolution=0.0f);

	// Create a null SOP with a point cloud of the control points of InSplinesComponent.
	static bool CreateInputNodeForControlPoints(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		const HAPI_NodeId& InObjectNodeId,
		const FString& InNodeName,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap, 
		int32& InNextControlPointId,
		HAPI_NodeId& OutNodeId);

private:
	// Extract landscape splines data arrays: positions, and various attributes.
	static bool ExtractSplineData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		FHoudiniUnrealLandscapeSplinesData& OutSplinesData,
		EHoudiniUnrealLandscapeSplineCurve InExportCurve=EHoudiniUnrealLandscapeSplineCurve::Center,
		float InSplineResolution=0.0f);
		
	// landscape splines control points data arrays: positions, rotations, and various attributes.
	static bool ExtractSplineControlPointsData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		FHoudiniUnrealLandscapeSplinesControlPointData& OutSplinesControlPointData);

	// Adds the landscape spline target landscape prim attribute (target = InLandscapeActor).
	static bool AddTargetLandscapeAttribute(
		HAPI_NodeId InNodeId,
		HAPI_PartId InPartId,
		ALandscapeProxy const* const InLandscapeActor,
		int InCount,
		HAPI_AttributeOwner InAttribOwner=HAPI_ATTROWNER_PRIM);

	static bool AddOutputAttribute(HAPI_NodeId InNodeId, HAPI_PartId InPartId, int InValue, int InCount,HAPI_AttributeOwner InAttribOwner=HAPI_ATTROWNER_PRIM);
	static bool AddPositionAttribute(HAPI_NodeId InNodeId, const TArray<float>& InPositions);
	static bool AddPaintLayerNameAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InPaintLayerNames, HAPI_AttributeOwner InAttribOwner);
	static bool AddRaiseTerrainAttribute(HAPI_NodeId InNodeId, const TArray<int8>& InRaiseTerrain, HAPI_AttributeOwner InAttribOwner);
	static bool AddLowerTerrainAttribute(HAPI_NodeId InNodeId, const TArray<int8>& InLowerTerrain, HAPI_AttributeOwner InAttribOwner);
	static bool AddSegmentMeshesAttributes(HAPI_NodeId InNodeId, const TArray<FHoudiniUnrealLandscapeSplineSegmentMeshData>& InPerMeshSegmentData);
	static bool AddConnectionSocketNameAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InPointConnectionSocketNames);
	static bool AddRotationAttribute(HAPI_NodeId InNodeId, const TArray<float>& InControlPointRotations);
	static bool AddMeshAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InMeshRefs);
	static bool AddMaterialOverrideAttributes(HAPI_NodeId InNodeId, const TArray<TArray<FString>>& InMaterialOverrideRefs);
	static bool AddIdsAttribute(HAPI_NodeId InNodeId, const TArray<int32>& InControlPointIds);
	static bool AddHalfWidthAttribute(HAPI_NodeId InNodeId, const TArray<float>& InHalfWidths);
	static bool AddSideFalloffAttribute(HAPI_NodeId InNodeId, const TArray<float>& InSideFalloffs);
	static bool AddEndFalloffAttribute(HAPI_NodeId InNodeId, const TArray<float>& InEndFalloffs);
	static bool AddTangentLengthAttribute(HAPI_NodeId InNodeId, const TArray<float>& InTangentLengths);
	static bool AddMeshScaleAttribute(HAPI_NodeId InNodeId, const TArray<float>& InTangentLengths);
	static bool AddControlPointAttributes(HAPI_NodeId InNodeId, const FHoudiniUnrealLandscapeSplineControlPointAttributes& InControlPointAttributes);
};

