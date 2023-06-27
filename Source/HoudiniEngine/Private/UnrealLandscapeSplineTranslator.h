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

#include "HAPI/HAPI_Common.h"
#include "Landscape/Classes/LandscapeSplinesComponent.h"

class ALandscapeProxy;
class ULandscapeSplinesComponent;
class UMaterialInterface;
class UStaticMesh;

class FUnrealObjectInputHandle;


enum class EHoudiniLandscapeSplineCurve : uint8
{
	Invalid = 0,
	Center = 1,
	Left = 2,
	Right = 3
};

struct FLandscapeSplineSegmentMeshData
{
	/** Per-segment mesh refs */
	TArray<FString> MeshRefs;
	
	/** Mesh material override, the outer index is material 0, 1, 2 ... the inner index is the segment index. */
	TArray<TArray<FString>> MeshMaterialOverrideRefs;

	/** Mesh scale. */
	TArray<float> MeshScales;
};

struct FLandscapeSplinesData
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
	TArray<FLandscapeSplineSegmentMeshData> PerMeshSegmentData;

	/**
	 * The mesh socket names on the splines' points, each index is a point index. Only the point indices that
	 * correspond to control points (first and last point of each segment) will have values set, the rest of the
	 * array will contain empty strings.
	 */
	TArray<FString> PointConnectionSocketNames;

	/**
	 * If a point corresponds with a control point on the spline, this contains the control point name. If the
	 * point does not correspond with a control point it'll be the empty string.
	 */
	TArray<FString> ControlPointNames;

	/** If a point corresponds with a control point on the spline, this contains the control point's half-width. */
	TArray<float> ControlPointHalfWidths;

	/**
	 * If a point corresponds with a control point on the spline, this contains the control point's tangent length
	 * for the segment connection.
	 */
	TArray<float> ControlPointTangentLengths;
};

struct FLandscapeSplinesControlPointData
{
	/**
	 * The control point positions of the splines. These are the original positions unaffected by connection mesh
	 * sockets.
	 */
	TArray<float> ControlPointPositions;

	/** Control point rotations. */
	TArray<float> ControlPointRotations;
	
	/** Control paint layer names */
	TArray<FString> ControlPointPaintLayerNames;
	
	/** Control bRaiseTerrain */
	TArray<int8> ControlPointRaiseTerrains;
	
	/** Control bLowerTerrain */
	TArray<int8> ControlPointLowerTerrains;

	/** The StaticMesh of each control point. */
	TArray<FString> ControlPointMeshRefs;

	/**
	 * The Material Override refs of each control point. The outer index material override index, the inner index
	 * is control point index.
	 */
	TArray<TArray<FString>> PerMaterialOverrideControlPointRefs;

	/** The static mesh scale of each control point. */
	TArray<float> ControlPointMeshScales;

	/** The names of the control points. */
	TArray<FString> ControlPointNames;

	/** The control point half-width. */
	TArray<float> ControlPointHalfWidths;
};


struct HOUDINIENGINE_API FUnrealLandscapeSplineTranslator 
{
public:
	/**
	 * @brief Create HAPI nodes and send the landscape splines of InLandscapeSplines to Houdini.
	 * @param InSplinesComponent The landscape splines component to translate to Houdini.
	 * @param OutInputNodeId The HAPI node id of the merge node that will be created. This can be set to the existing
	 * node by the caller, which will then be deleted and recreated.
	 * @param OutInputNodeHandle The input handle for the reference counted input system for the newly created HAPI node.
	 * @param InNodeName The base name to use for the HAPI nodes.
	 * @param bInExportCurves If true export the splines as curves.
	 * @param bInExportControlPoints If true, then create a separate control point point cloud and merge it with
	 * the splines / curves. This can be useful in cases where the control points in the splines are offset by mesh
	 * socket connections.
	 * @param bInInputNodesCanBeDeleted Set to true if deletion of input nodes should be allowed.
	 * @return True if the landscape splines were successfully sent to Houdini. In failure cases some HAPI nodes could
	 * still have been created, so OutInputNodeId and OutInputNodeHandle should still be handled appropriately.
	 */
	static bool CreateInputNodeForLandscapeSplinesComponent(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		HAPI_NodeId& OutInputNodeId,
		FUnrealObjectInputHandle& OutInputNodeHandle,
		const FString& InNodeName,
		const bool bInExportCurves=true,
		const bool bInExportControlPoints=false,
		const bool bInExportLeftRightCurves=false,
		const bool bInInputNodesCanBeDeleted=true);

	/**
	 * @brief Create a null SOP with a curve for each spline/segment of InSplinesComponent.
	 * @param InSplinesComponent The landscape splines component to send as curves to Houdini.
	 * @param InObjectNodeId The HAPI object geo node in which to create the node for the curves.
	 * @param InNodeName The basename to use for naming the spline curves node. The final node name as a "_splines" suffix.
	 * @param OutNodeId The HAPI node id of the SOP that was created for the curves.
	 * @return True if the operation was successful. In failure cases some HAPI nodes could still have been created, so
	 * OutNodeId must still be handled appropriately.
	 */
	static bool CreateInputNodeForLandscapeSplines(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		const HAPI_NodeId& InObjectNodeId,
		const FString& InNodeName,
		HAPI_NodeId& OutNodeId,
		const EHoudiniLandscapeSplineCurve InExportCurve=EHoudiniLandscapeSplineCurve::Center);

	/**
	 * @brief Create a null SOP with a point cloud of the control points of InSplinesComponent.
	 * @param InSplinesComponent The landscape splines component to create a point cloud from.
	 * @param InObjectNodeId The HAPI object geo node in which to create the node for the point cloud.
	 * @param InNodeName The basename to use for naming the spline control points node. The final node name as a "_control_points" suffix.
	 * @param OutNodeId The HAPI node id of the SOP that was created for the point cloud.
	 * @return True if the operation was successful. In failure cases some HAPI nodes could still have been created, so
	 * OutNodeId must still be handled appropriately.
	 */
	static bool CreateInputNodeForLandscapeSplinesControlPoints(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		const HAPI_NodeId& InObjectNodeId,
		const FString& InNodeName,
		HAPI_NodeId& OutNodeId);

protected:
	/**
	 * @brief Extract landscape splines data arrays: positions, and various attributes.
	 * @param InSplinesComponent The landscape splines component.
	 * @param OutSplinesData The struct to extract data into.
	 * @return True if data was successfully extracted.
	 */
	static bool ExtractLandscapeSplineData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		FLandscapeSplinesData& OutSplinesData,
		const EHoudiniLandscapeSplineCurve InExportCurve=EHoudiniLandscapeSplineCurve::Center);
		
	/**
	 * @brief Extract landscape splines control points data arrays: positions, rotations, and various attributes.
	 * @param InSplinesComponent The landscape splines component.
	 * @param OutSplinesControlPointData The struct to extract data into.
	 * @return True if data was successfully extracted.
	 */
	static bool ExtractLandscapeSplineControlPointData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		FLandscapeSplinesControlPointData& OutSplinesControlPointData);

	/**
	 * Adds the landscape spline target landscape prim attribute (target = InLandscapeActor).
	 * @param InNodeId The HAPI node that contains the geo create the attribute on.
	 * @param InPartId The part id containing the geo.
	 * @param InLandscapeActor The landscape actor to set as target.
	 * @param InCount The number of prims in the part.
	 */
	static bool AddLandscapeSplineTargetLandscapeAttribute(
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		ALandscapeProxy const* const InLandscapeActor,
		const int32 InCount,
		const HAPI_AttributeOwner InAttribOwner=HAPI_ATTROWNER_PRIM);

	/**
	 * Adds the landscape splines output attribute to identify a curve as a landscape spline.
	 * @param InNodeId The HAPI node that contains the geo create the attribute on.
	 * @param InPartId The part id containing the geo.
	 * @param InValue The value to write. The meaning of the value is the corresponding EHoudiniLandscapeSplineCurve
	 * enum value.
	 * @param InCount The number of elements (points, prims etc, see InAttribOwner) in the part.
	 * @param InAttribOwner The type of attribute: point, prim etc.
	 */
	static bool AddLandscapeSplineOutputAttribute(
		const HAPI_NodeId& InNodeId,
		const HAPI_PartId& InPartId,
		const int32 InValue,
		const int32 InCount,
		const HAPI_AttributeOwner InAttribOwner=HAPI_ATTROWNER_PRIM);

	static bool AddLandscapeSplinePositionAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InPositions);

	static bool AddLandscapeSplinePaintLayerNameAttribute(
		const HAPI_NodeId& InNodeId, const TArray<FString>& InPaintLayerNames, const HAPI_AttributeOwner InAttribOwner);
	
	static bool AddLandscapeSplineRaiseTerrainAttribute(
		const HAPI_NodeId& InNodeId, const TArray<int8>& InRaiseTerrain, const HAPI_AttributeOwner InAttribOwner);

	static bool AddLandscapeSplineLowerTerrainAttribute(
		const HAPI_NodeId& InNodeId, const TArray<int8>& InLowerTerrain, const HAPI_AttributeOwner InAttribOwner);

	static bool AddLandscapeSplineSegmentMeshesAttributes(
		const HAPI_NodeId& InNodeId, const TArray<FLandscapeSplineSegmentMeshData>& InPerMeshSegmentData);

	static bool AddLandscapeSplineConnectionSocketNameAttribute(
		const HAPI_NodeId& InNodeId, const TArray<FString>& InPointConnectionSocketNames);

	static bool AddLandscapeSplineControlPointRotationAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InControlPointRotations);

	static bool AddLandscapeSplineControlPointMeshAttribute(
		const HAPI_NodeId& InNodeId, const TArray<FString>& InMeshRefs);

	static bool AddLandscapeSplineControlPointMaterialOverrideAttributes(
		const HAPI_NodeId& InNodeId, const TArray<TArray<FString>>& InMaterialOverrideRefs);

	static bool AddLandscapeSplineControlPointNamesAttribute(
		const HAPI_NodeId& InNodeId, const TArray<FString>& InControlPointNames);

	static bool AddLandscapeSplineHalfWidthAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InHalfWidths);

	static bool AddLandscapeSplineTangentLengthAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InTangentLengths);

	static bool AddLandscapeSplineControlPointMeshScaleAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InTangentLengths);
};
