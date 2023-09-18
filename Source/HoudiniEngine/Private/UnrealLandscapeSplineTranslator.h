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

class FLandscapeSplineControlPointAttributes;
struct FLandscapeSplineInterpPoint;
class ALandscapeProxy;
class ULandscapeSplinesComponent;
class UMaterialInterface;
class UStaticMesh;

class FUnrealObjectInputHandle;
struct FLandscapeSplineSegmentMeshData;
struct FLandscapeSplinesControlPointData;
struct FLandscapeSplinesData;


enum class EHoudiniLandscapeSplineCurve : uint8
{
	Invalid = 0,
	Center = 1,
	Left = 2,
	Right = 3
};


struct HOUDINIENGINE_API FUnrealLandscapeSplineTranslator 
{
public:
	/**
	 * @brief Create HAPI nodes and send the landscape splines of InLandscapeSplines to Houdini.
	 * @param InSplinesComponent The landscape splines component to translate to Houdini.
	 * @param bForceReferenceInputNodeCreation Only applicable to the new input system. If True this function creates
	 * the reference node even if only bInExport option is True.
	 * @param OutInputNodeId The HAPI node id of the merge node that will be created. This can be set to the existing
	 * node by the caller, which will then be deleted and recreated.
	 * @param OutInputNodeHandle The input handle for the reference counted input system for the newly created HAPI node.
	 * @param InNodeName The base name to use for the HAPI nodes.
	 * @param InSplineResolution The spline resolution. <= 0 leaves the landscape spline at its current resolution.
	 * @param bInExportCurves If true export the splines as curves.
	 * @param bInExportControlPoints If true, then create a separate control point point cloud and merge it with
	 * the splines / curves. This can be useful in cases where the control points in the splines are offset by mesh
	 * socket connections.
	 * @param bInExportLeftRightCurves Export curves for the .Left and .Right positions of each interpolated spline
	 * point as well.
	 * @param bInInputNodesCanBeDeleted Set to true if deletion of input nodes should be allowed.
	 * @return True if the landscape splines were successfully sent to Houdini. In failure cases some HAPI nodes could
	 * still have been created, so OutInputNodeId and OutInputNodeHandle should still be handled appropriately.
	 */
	static bool CreateInputNodeForLandscapeSplinesComponent(
		ULandscapeSplinesComponent* const InSplinesComponent,
		const bool bForceReferenceInputNodeCreation,
		HAPI_NodeId& OutInputNodeId,
		FUnrealObjectInputHandle& OutInputNodeHandle,
		const FString& InNodeName,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		const float InSplineResolution=0.0f,
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
	 * @param InExportCurve The curve to export: Center (default), Left or Right.
	 * @param InSplineResolution The spline resolution. <= 0 leaves the landscape spline at its current resolution.
	 * @return True if the operation was successful. In failure cases some HAPI nodes could still have been created, so
	 * OutNodeId must still be handled appropriately.
	 */
	static bool CreateInputNodeForLandscapeSplines(
		ULandscapeSplinesComponent* const InSplinesComponent, 
		const HAPI_NodeId& InObjectNodeId,
		const FString& InNodeName,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		HAPI_NodeId& OutNodeId,
		const EHoudiniLandscapeSplineCurve InExportCurve=EHoudiniLandscapeSplineCurve::Center,
		const float InSplineResolution=0.0f);

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
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		HAPI_NodeId& OutNodeId);

private:
	/**
	 * @brief Extract landscape splines data arrays: positions, and various attributes.
	 * @param InSplinesComponent The landscape splines component.
	 * @param OutSplinesData The struct to extract data into.
	 * @param InExportCurve The curve to export: Center (default), Left or Right.
	 * @param InSplineResolution The spline resolution. <= 0 leaves the landscape spline at its current resolution.
	 * @return True if data was successfully extracted.
	 */
	static bool ExtractLandscapeSplineData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
		FLandscapeSplinesData& OutSplinesData,
		const EHoudiniLandscapeSplineCurve InExportCurve=EHoudiniLandscapeSplineCurve::Center,
		const float InSplineResolution=0.0f);
		
	/**
	 * @brief Extract landscape splines control points data arrays: positions, rotations, and various attributes.
	 * @param InSplinesComponent The landscape splines component.
	 * @param OutSplinesControlPointData The struct to extract data into.
	 * @return True if data was successfully extracted.
	 */
	static bool ExtractLandscapeSplineControlPointsData(
		ULandscapeSplinesComponent* const InSplinesComponent,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId,
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

	static bool AddLandscapeSplineControlPointIdsAttribute(
		const HAPI_NodeId& InNodeId, const TArray<int32>& InControlPointIds);

	static bool AddLandscapeSplineHalfWidthAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InHalfWidths);

	static bool AddLandscapeSplineSideFalloffAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InSideFalloffs);

	static bool AddLandscapeSplineEndFalloffAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InEndFalloffs);

	static bool AddLandscapeSplineTangentLengthAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InTangentLengths);

	static bool AddLandscapeSplineControlPointMeshScaleAttribute(
		const HAPI_NodeId& InNodeId, const TArray<float>& InTangentLengths);

	static bool AddLandscapeSplineControlPointAttributes(
		const HAPI_NodeId& InNodeId, const FLandscapeSplineControlPointAttributes& InControlPointAttributes);
};
