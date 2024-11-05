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

#include "UnrealLandscapeSplineTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"

#include "Landscape.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "Materials/MaterialInterface.h"


/**
 * Helper struct to record segment data, such as the segment length, unresampled points (spline points generated in UE),
 * and global segment index (index in the output arrays of data sent to Houdini).
 */
struct FOrderedSegmentData
{
	TObjectPtr<ULandscapeSplineSegment> Segment;
	float SegmentLength = 0.0f;
	TArray<FHoudiniUnResampledPoint> UnResampledPoints;
	int32 GlobalSegmentIndex = INDEX_NONE;
};

/**
 * Helper struct to record segments that are connected and have the same orientation.
 */
struct FConnectedSpline
{
	TArray<FOrderedSegmentData> OrderedSegments;
	ULandscapeSplineControlPoint* Start;
	ULandscapeSplineControlPoint* End;
};


void
FHoudiniUnrealLandscapeSplineControlPointAttributes::Init(const int32 InExpectedPointCount)
{
	PointCount = InExpectedPointCount;
	Rotations.Empty(PointCount * 4);
	PaintLayerNames.Empty(PointCount);
	RaiseTerrains.Empty(PointCount);
	LowerTerrains.Empty(PointCount);
	MeshRefs.Empty(PointCount);
	MaterialOverrideRefs.Empty();
	MeshScales.Empty(PointCount * 3);
	Ids.Empty(PointCount);
	HalfWidths.Empty(PointCount);
	SideFalloffs.Empty(PointCount);
	EndFalloffs.Empty(PointCount);
}

void
ConvertAndSetRotation(const FRotator& InUnrealRotation, const int32 InArrayStartIndex, TArray<float>& OutQuatFloatArray)
{
	// Convert Unreal X-Forward to Houdini Z-Forward and Unreal Z-Up to Houdini Y-Up
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0)
	static constexpr float HalfPI = UE_HALF_PI;
#else
	static constexpr float HalfPI = HALF_PI;
#endif
	const FQuat CPRot = InUnrealRotation.Quaternion() * FQuat(FVector::UpVector, -HalfPI);
	if (InArrayStartIndex >= 0)
	{
		check(OutQuatFloatArray.IsValidIndex(InArrayStartIndex + 3));
		OutQuatFloatArray[InArrayStartIndex + 0] = CPRot.X;
		OutQuatFloatArray[InArrayStartIndex + 1] = CPRot.Z;
		OutQuatFloatArray[InArrayStartIndex + 2] = CPRot.Y;
		OutQuatFloatArray[InArrayStartIndex + 3] = -CPRot.W;
	}
	else
	{
		OutQuatFloatArray.Add(CPRot.X);
		OutQuatFloatArray.Add(CPRot.Z);
		OutQuatFloatArray.Add(CPRot.Y);	
		OutQuatFloatArray.Add(-CPRot.W);
	}
}

bool
FHoudiniUnrealLandscapeSplineControlPointAttributes::AddControlPointData(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	TObjectPtr<ULandscapeSplineControlPoint>& InControlPoint,
#else
	const ULandscapeSplineControlPoint * InControlPoint,
#endif
	int32 InControlPointIndex,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId)
{
	if (!IsValid(InControlPoint))
		return false;

	ConvertAndSetRotation(InControlPoint->Rotation, -1, Rotations); 

	const int32 ControlPointId = FHoudiniLandscapeRuntimeUtils::GetOrGenerateValidControlPointId(
		InControlPoint, InControlPointIdMap, InNextControlPointId);
	Ids.Emplace(ControlPointId);

	HalfWidths.Add(InControlPoint->Width / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	SideFalloffs.Add(InControlPoint->SideFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	EndFalloffs.Add(InControlPoint->EndFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	PaintLayerNames.Emplace(InControlPoint->LayerName.ToString());
	RaiseTerrains.Add(InControlPoint->bRaiseTerrain);
	LowerTerrains.Add(InControlPoint->bLowerTerrain);

	// Set the static mesh reference 
	MeshRefs.Emplace(IsValid(InControlPoint->Mesh) ? InControlPoint->Mesh->GetPathName() : FString());

	const int32 NumMaterialOverrides = InControlPoint->MaterialOverrides.Num();
	if (MaterialOverrideRefs.Num() < NumMaterialOverrides)
		MaterialOverrideRefs.SetNum(NumMaterialOverrides);
	for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
	{
		UMaterialInterface const* const Material = InControlPoint->MaterialOverrides[MaterialOverrideIdx];
		// Initialize the per control point array for this override index if necessary
		TArray<FString>& PerCPMaterialOverrideRefs = MaterialOverrideRefs[MaterialOverrideIdx];
		if (PerCPMaterialOverrideRefs.IsEmpty())
			PerCPMaterialOverrideRefs.SetNum(PointCount);

		// Set the material ref or empty string if the material is invalid
		PerCPMaterialOverrideRefs[InControlPointIndex] = IsValid(Material) ? Material->GetPathName() : FString(); 
	}

	MeshScales.Add(InControlPoint->MeshScale.X);
	MeshScales.Add(InControlPoint->MeshScale.Z);
	MeshScales.Add(InControlPoint->MeshScale.Y);

	return true;
}


void
FHoudiniUnrealLandscapeSplineControlPointAttributes::AddEmpty()
{
	Rotations.Add(FQuat::Identity.X);
	Rotations.Add(FQuat::Identity.Z);
	Rotations.Add(FQuat::Identity.Y);
	Rotations.Add(-FQuat::Identity.W);

	Ids.Add(INDEX_NONE);

	HalfWidths.AddDefaulted();
	SideFalloffs.AddDefaulted();
	EndFalloffs.AddDefaulted();

	PaintLayerNames.AddDefaulted();
	RaiseTerrains.Add(false);
	LowerTerrains.Add(false);

	MeshRefs.AddDefaulted();

	MeshScales.Add(1.0f);
	MeshScales.Add(1.0f);
	MeshScales.Add(1.0f);
}


FHoudiniUnResampledPoint::FHoudiniUnResampledPoint(EHoudiniUnrealLandscapeSplineCurve InSplineSelection)
	: Alpha(0.0f)
	, SplineSelection(InSplineSelection)
{
}

FHoudiniUnResampledPoint::FHoudiniUnResampledPoint(EHoudiniUnrealLandscapeSplineCurve InSplineSelection, const FLandscapeSplineInterpPoint& InPoint)
	: Center(InPoint.Center)
	, Left(InPoint.Left)
	, Right(InPoint.Right)
	, FalloffLeft(InPoint.FalloffLeft)
	, FalloffRight(InPoint.FalloffRight)
	, Rotation(FQuat::Identity)
	, Alpha(0.0f)
	, SplineSelection(InSplineSelection)
{
	
}


FVector
FHoudiniUnResampledPoint::GetSelectedPosition() const
{
	switch (SplineSelection)
	{
	case EHoudiniUnrealLandscapeSplineCurve::Center:
		return Center;
	case EHoudiniUnrealLandscapeSplineCurve::Left:
		return Left;
	case EHoudiniUnrealLandscapeSplineCurve::Right:
		return Right;
	default:
		HOUDINI_LOG_WARNING(TEXT("Invalid value for SplineSelection: %d, returning Center point."), SplineSelection);
		break;
	}
	
	return Center;
}

FQuat
FHoudiniUnResampledPoint::CalculateRotationTo(const FHoudiniUnResampledPoint& InNextPoint)
{
	FVector ForwardVector = (InNextPoint.Center - Center).GetSafeNormal();
	FVector RightVector = (Right - Center).GetSafeNormal();
	Rotation = FRotationMatrix::MakeFromXY(ForwardVector, RightVector).ToQuat();
	return Rotation;
}

bool
FUnrealLandscapeSplineTranslator::CreateInputNode(
	ULandscapeSplinesComponent* const InSplinesComponent, 
	bool bForceReferenceInputNodeCreation,
	HAPI_NodeId& OutCreatedInputNodeId,
	FUnrealObjectInputHandle& OutInputNodeHandle,
	const FString& InNodeName,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	float InSplineResolution,
	bool bInExportCurves,
	bool bInExportControlPoints,
	bool bInExportLeftRightCurves,
	bool bInInputNodesCanBeDeleted)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Input node name, defaults to InNodeName, but can be changed by the new input system
	FString FinalInputNodeName = InNodeName;

	// Find the node in new input system
	// Identifier will be the identifier for the entry created in this call of the function.
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;
	{
		// Check if we already have an input node for this component and its options
		bool bSingleLeafNodeOnly = false;
		FUnrealObjectInputIdentifier IdentReferenceNode;
		TArray<FUnrealObjectInputIdentifier> IdentPerOption;
		if (!FUnrealObjectInputUtils::BuildLandscapeSplinesInputObjectIdentifiers(
			InSplinesComponent,
			bInExportCurves,
			bInExportControlPoints,
			bInExportLeftRightCurves,
			InSplineResolution,
			bForceReferenceInputNodeCreation,
			bSingleLeafNodeOnly,
			IdentReferenceNode,
			IdentPerOption))
		{
			return false;
		}

		if (bSingleLeafNodeOnly)
		{
			// We'll create the splines input node entirely is this function call
			check(!IdentPerOption.IsEmpty());
			Identifier = IdentPerOption[0];
		}
		else
		{
			// Look for the reference node that references the per-option (curves, control points) nodes
			Identifier = IdentReferenceNode;
		}
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId) && (bSingleLeafNodeOnly || FUnrealObjectInputUtils::AreReferencedHAPINodesValid(Handle)))
			{
				if (!bInInputNodesCanBeDeleted)
				{
					// Make sure to prevent deletion of the input node if needed
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInInputNodesCanBeDeleted);
				}

				OutInputNodeHandle = Handle;
				OutCreatedInputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// We now need to create the nodes (since we couldn't find existing ones in the manager)
		// For the single leaf node case we can simply continue this function
		// For the ref + multiple options, we call this function again for each option (as a single leaf node) and
		// then create the reference node.
		if (!bSingleLeafNodeOnly)
		{
			TSet<FUnrealObjectInputHandle> PerOptionNodeHandles;
			PerOptionNodeHandles.Reserve(IdentPerOption.Num());
			for (const FUnrealObjectInputIdentifier& OptionIdentifier : IdentPerOption)
			{
				FUnrealObjectInputHandle OptionHandle;
				const FUnrealObjectInputOptions& Options = OptionIdentifier.GetOptions();
				HAPI_NodeId NewNodeId = -1;
				FString NodeLabel;
				FUnrealObjectInputUtils::GetDefaultInputNodeName(OptionIdentifier, NodeLabel);

				if (FUnrealObjectInputUtils::FindNodeViaManager(OptionIdentifier, OptionHandle))
				{
					// The node already exists, but it is dirty. Fetch its HAPI node ID so that the old
					// node can be deleted when creating the new HAPI node.
					// TODO: maybe the new input system manager should delete the old HAPI nodes when we set the new
					//		 HAPI node IDs on the node entries in the manager?
					FUnrealObjectInputUtils::GetHAPINodeId(OptionHandle, NewNodeId);
				}

				static constexpr bool bForceInputRefNodeCreation = false;
				if (!CreateInputNode(
						InSplinesComponent,
						bForceInputRefNodeCreation,
						NewNodeId,
						OptionHandle,
						NodeLabel,
						InControlPointIdMap,
						InNextControlPointId,
						Options.UnrealSplineResolution,
						!Options.bExportLandscapeSplineControlPoints && !Options.bExportLandscapeSplineLeftRightCurves,
						Options.bExportLandscapeSplineControlPoints,
						Options.bExportLandscapeSplineLeftRightCurves,
						bInInputNodesCanBeDeleted))
				{
					return false;
				}

				PerOptionNodeHandles.Add(OptionHandle);
			}

			// Create or update the HAPI node for the reference node if it does not exist
			FUnrealObjectInputHandle RefNodeHandle;
			if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(IdentReferenceNode, PerOptionNodeHandles, RefNodeHandle, true, bInInputNodesCanBeDeleted))
				return false;
			
			OutInputNodeHandle = RefNodeHandle;
			FUnrealObjectInputUtils::GetHAPINodeId(IdentReferenceNode, OutCreatedInputNodeId);
			return true;
		}

		// Set OutCreatedInputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that OutCreatedInputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, OutCreatedInputNodeId))
				OutCreatedInputNodeId = -1;
		}
		else
		{
			OutCreatedInputNodeId = -1;
		}
	}

	HAPI_NodeId PreviousInputNodeId = OutCreatedInputNodeId;

	// Delete the previous nodes, if valid
	if (PreviousInputNodeId >= 0 && FHoudiniEngineUtils::IsHoudiniNodeValid(PreviousInputNodeId))
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);
		}

		if (PreviousInputOBJNode >= 0)
		{
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *FinalInputNodeName);
			}
		}
	}

	int32 NumNodesNeeded = 0;
	if (bInExportCurves)
		NumNodesNeeded++;
	if (bInExportControlPoints)
		NumNodesNeeded++;
	if (bInExportLeftRightCurves)
		NumNodesNeeded += 2;
	
	const bool bUseMergeNode = NumNodesNeeded > 1;
	HAPI_NodeId NewNodeId = -1;
	HAPI_NodeId ObjectNodeId = -1;
	// Create geo node
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniEngineUtils::CreateNode(ParentNodeId, ParentNodeId < 0 ? TEXT("Object/geo") : TEXT("geo"), FinalInputNodeName, false, &ObjectNodeId), false);
	// Check if we have a valid id for the new geo obj
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(ObjectNodeId))
		return false;

	if (bUseMergeNode)
	{
		// Create merge sop in geo obj
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniEngineUtils::CreateNode(ObjectNodeId, TEXT("merge"), FinalInputNodeName, false, &NewNodeId), false);
		// Update our input NodeId
		OutCreatedInputNodeId = NewNodeId;
		
		// Check if we have a valid id for this new input asset.
		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
			return false;
	}
	
	bool bSuccess = true;
	int32 MergeNodeInputIdx = 0;
	if (bInExportCurves)
	{
		HAPI_NodeId SplinesNodeId = -1;
		if (!CreateInputNode(
				InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
				SplinesNodeId,	EHoudiniUnrealLandscapeSplineCurve::Center, InSplineResolution))
		{
			bSuccess = false;
		}
		else
		{
			if (!bUseMergeNode)
			{
				OutCreatedInputNodeId = SplinesNodeId;
			}
			else
			{
				// Connect to the merge node
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					OutCreatedInputNodeId, MergeNodeInputIdx, SplinesNodeId, 0), false);
				MergeNodeInputIdx++;
			}
		}
	}

	if (bInExportControlPoints)
	{
		HAPI_NodeId ControlPointCloudNodeId = -1;
		if (!CreateInputNodeForControlPoints(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId, ControlPointCloudNodeId))
		{
			bSuccess = false;
		}
		else
		{
			if (!bUseMergeNode)
			{
				OutCreatedInputNodeId = ControlPointCloudNodeId;
			}
			else
			{
				// Connect to the merge node
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					OutCreatedInputNodeId, MergeNodeInputIdx, ControlPointCloudNodeId, 0), false);
				MergeNodeInputIdx++;
			}
		}
	}

	if (bInExportLeftRightCurves)
	{
		HAPI_NodeId SplinesNodeId = -1;
		if (!CreateInputNode(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
			SplinesNodeId, EHoudiniUnrealLandscapeSplineCurve::Left, InSplineResolution))
		{
			bSuccess = false;
		}
		else
		{
			// Connect to the merge node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				OutCreatedInputNodeId, MergeNodeInputIdx, SplinesNodeId, 0), false);
			MergeNodeInputIdx++;
		}
		if (!CreateInputNode(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
			SplinesNodeId, EHoudiniUnrealLandscapeSplineCurve::Right, InSplineResolution))
		{
			bSuccess = false;
		}
		else
		{
			// Connect to the merge node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				OutCreatedInputNodeId, MergeNodeInputIdx, SplinesNodeId, 0), false);
			MergeNodeInputIdx++;
		}
	}

	{
		// Get our parent OBJ NodeID
		const HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(OutCreatedInputNodeId);
		static constexpr TSet<FUnrealObjectInputHandle> const* ReferencedNodes = nullptr; 
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, OutCreatedInputNodeId, Handle, InputObjectNodeId, ReferencedNodes, bInInputNodesCanBeDeleted))
			OutInputNodeHandle = Handle;
	}

	return bSuccess;
}


bool
FUnrealLandscapeSplineTranslator::CreateInputNode(
	ULandscapeSplinesComponent* const InSplinesComponent,
	HAPI_NodeId InObjectNodeId,
	const FString& InNodeName,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	HAPI_NodeId& OutNodeId,
	const EHoudiniUnrealLandscapeSplineCurve InExportCurve,
	const float InSplineResolution)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Input node name: InNodeName + a suffix describing if this is center or left/right curves.
	FString FinalInputNodeName;
	switch (InExportCurve)
	{
	case EHoudiniUnrealLandscapeSplineCurve::Center:
		FinalInputNodeName = InNodeName + TEXT("_curves");
		break;
	case EHoudiniUnrealLandscapeSplineCurve::Left:
		FinalInputNodeName = InNodeName + TEXT("_left_curves");
		break;
	case EHoudiniUnrealLandscapeSplineCurve::Right:
		FinalInputNodeName = InNodeName + TEXT("_right_curves");
		break;
	}

	FHoudiniUnrealLandscapeSplinesData SplinesData;
	if (!ExtractSplineData(InSplinesComponent, InControlPointIdMap, InNextControlPointId, SplinesData, InExportCurve, InSplineResolution))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to extract landscape splines data."));
		return false;
	}

	// Extract linear position array and calculate number of vertices
	const int32 NumSegments = SplinesData.VertexCounts.Num();
	const int32 NumVerts = SplinesData.PointPositions.Num() / 3;
	
	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

	// Create null sop in geo obj
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(InObjectNodeId, TEXT("null"), FinalInputNodeName, false, &OutNodeId), false);
	
	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(OutNodeId))
		return false;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	PartInfo.id = 0;
	PartInfo.nameSH = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	PartInfo.type = HAPI_PARTTYPE_CURVE;
	PartInfo.pointCount = SplinesData.PointPositions.Num() / 3;
	PartInfo.vertexCount = NumVerts; 
	PartInfo.faceCount = NumSegments;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(Session, OutNodeId, 0, &PartInfo), false);

	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	CurveInfo.curveType = HAPI_CURVETYPE_LINEAR;
	CurveInfo.curveCount = NumSegments;
	CurveInfo.vertexCount = NumVerts; 
	CurveInfo.knotCount = 0; 
	CurveInfo.isPeriodic = false;
	CurveInfo.isRational = false; 
	CurveInfo.order = 0;
	CurveInfo.hasKnots = false;
	CurveInfo.isClosed = false;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveInfo(Session, OutNodeId, 0, &CurveInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveCounts(
		Session, OutNodeId, 0, SplinesData.VertexCounts.GetData(),
		0, SplinesData.VertexCounts.Num()), false);
	
	// Add attributes
	bool bNeedToCommit = false;

	// Point attributes
	if (AddPositionAttribute(OutNodeId, SplinesData.PointPositions))
		bNeedToCommit = true;

	if (AddControlPointAttributes(OutNodeId, SplinesData.ControlPointAttributes))
		bNeedToCommit = true;

	// Segment attributes
	if (AddPaintLayerNameAttribute(OutNodeId, SplinesData.SegmentPaintLayerNames, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddRaiseTerrainAttribute(OutNodeId, SplinesData.SegmentRaiseTerrains, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddLowerTerrainAttribute(OutNodeId, SplinesData.SegmentLowerTerrains, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddSegmentMeshesAttributes(OutNodeId, SplinesData.PerMeshSegmentData))
		bNeedToCommit = true;

	// Segment connection attributes (point attributes)
	if (AddTangentLengthAttribute(OutNodeId, SplinesData.PointConnectionTangentLengths))
		bNeedToCommit = true;

	if (AddConnectionSocketNameAttribute(OutNodeId, SplinesData.PointConnectionSocketNames))
		bNeedToCommit = true;

	// Add the unreal_landscape_spline_output attribute to indicate that this a landscape spline and not a normal curve
	if (AddOutputAttribute(OutNodeId, 0, static_cast<int32>(InExportCurve), PartInfo.faceCount))
		bNeedToCommit = true;

	// Add landscape spline component tags if it has any
	if (FHoudiniEngineUtils::CreateGroupsFromTags(OutNodeId, 0, InSplinesComponent->ComponentTags))
		bNeedToCommit = true;
	
	// Add the parent actor's tag if it has any
	AActor* const ParentActor = InSplinesComponent->GetOwner();
	if (IsValid(ParentActor)) 
	{
		if (FHoudiniEngineUtils::CreateGroupsFromTags(OutNodeId, 0, ParentActor->Tags))
			bNeedToCommit = true;

		// Add the unreal_actor_path attribute
		if (FHoudiniEngineUtils::AddActorPathAttribute(OutNodeId, 0, ParentActor, PartInfo.faceCount))
			bNeedToCommit = true;
		
		// Add the unreal_level_path attribute
		if(FHoudiniEngineUtils::AddLevelPathAttribute(OutNodeId, 0, ParentActor->GetLevel(), PartInfo.faceCount))
			bNeedToCommit = true;

		// Should be attached to a landscape...
		ALandscapeSplineActor const* const SplinesActor = Cast<ALandscapeSplineActor>(ParentActor); 
		if (IsValid(SplinesActor))
		{
			ULandscapeInfo const* const LandscapeInfo = SplinesActor->GetLandscapeInfo();
			if (IsValid(LandscapeInfo) && LandscapeInfo->LandscapeActor.IsValid())
			{
				// Add the unreal_landscape_spline_target_landscape attribute
				if (AddTargetLandscapeAttribute(OutNodeId, 0, LandscapeInfo->LandscapeActor.Get(), PartInfo.faceCount))
					bNeedToCommit = true;
			}
		}
	}

	if (bNeedToCommit) 
	{
		// We successfully added tags to the geo, so we need to commit the changes
		if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiCommitGeo(OutNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not create groups for the landscape spline input's tags!"));
			return false;
		}

		// And cook it with refinement disabled (we want to strictly keep the control points and segments as they are)
		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		CookOptions.maxVerticesPerPrimitive = -1;
		CookOptions.refineCurveToLinear = false;
		static constexpr bool bWaitForCompletion = false;
		if (!FHoudiniEngineUtils::HapiCookNode(OutNodeId, &CookOptions, bWaitForCompletion))
			return false;
	}

	return true;
}


bool
FUnrealLandscapeSplineTranslator::CreateInputNodeForControlPoints(
	ULandscapeSplinesComponent* const InSplinesComponent,
	const HAPI_NodeId& InObjectNodeId,
	const FString& InNodeName,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	HAPI_NodeId& OutNodeId)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Set the final node name with _control_points suffix
	const FString FinalInputNodeName = InNodeName + TEXT("_control_points");

	FHoudiniUnrealLandscapeSplinesControlPointData ControlPointsData;
	if (!ExtractSplineControlPointsData(InSplinesComponent, InControlPointIdMap, InNextControlPointId, ControlPointsData))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to extract landscape splines control points data."));
		return false;
	}

	const int32 NumPoints = ControlPointsData.ControlPointPositions.Num() / 3;
	
	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

	// Create null sop in geo obj
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniEngineUtils::CreateNode(InObjectNodeId, TEXT("null"), FinalInputNodeName, false, &OutNodeId), false);
	
	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(OutNodeId))
		return false;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	PartInfo.id = 0;
	PartInfo.nameSH = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	PartInfo.type = HAPI_PARTTYPE_MESH;
	PartInfo.pointCount = NumPoints;
	PartInfo.vertexCount = 0; 
	PartInfo.faceCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(Session, OutNodeId, 0, &PartInfo), false);

	// Add attributes
	bool bNeedToCommit = false;

	if (AddPositionAttribute(OutNodeId, ControlPointsData.ControlPointPositions))
		bNeedToCommit = true;

	if (AddControlPointAttributes(OutNodeId, ControlPointsData.Attributes))
		bNeedToCommit = true;

	// Add the unreal_landscape_spline_output attribute to indicate that this a landscape spline and not a normal curve
	// TODO: Should there be a special type / value for the control points
	if (AddOutputAttribute(OutNodeId, 0, 1, PartInfo.pointCount, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	// // Add landscape spline component tags if it has any
	// if (FHoudiniEngineUtils::CreateGroupsFromTags(OutNodeId, 0, InSplinesComponent->ComponentTags))
	// 	bNeedToCommit = true;
	
	// Add the parent actor's tag if it has any
	AActor* const ParentActor = InSplinesComponent->GetOwner();
	if (IsValid(ParentActor)) 
	{
		// if (FHoudiniEngineUtils::CreateGroupsFromTags(OutNodeId, 0, ParentActor->Tags))
		// 	bNeedToCommit = true;
	
		// Add the unreal_actor_path attribute
		if (FHoudiniEngineUtils::AddActorPathAttribute(OutNodeId, 0, ParentActor, PartInfo.pointCount, HAPI_ATTROWNER_POINT))
			bNeedToCommit = true;
		
		// Add the unreal_level_path attribute
		if(FHoudiniEngineUtils::AddLevelPathAttribute(OutNodeId, 0, ParentActor->GetLevel(), PartInfo.pointCount, HAPI_ATTROWNER_POINT))
			bNeedToCommit = true;
	
		// Should be attached to a landscape...
		ALandscapeSplineActor const* const SplinesActor = Cast<ALandscapeSplineActor>(ParentActor); 
		if (IsValid(SplinesActor))
		{
			ULandscapeInfo const* const LandscapeInfo = SplinesActor->GetLandscapeInfo();
			if (IsValid(LandscapeInfo) && LandscapeInfo->LandscapeActor.IsValid())
			{
				// Add the unreal_landscape_spline_target_landscape attribute
				if (AddTargetLandscapeAttribute(OutNodeId, 0, LandscapeInfo->LandscapeActor.Get(), PartInfo.pointCount, HAPI_ATTROWNER_POINT))
					bNeedToCommit = true;
			}
		}
	}

	if (bNeedToCommit) 
	{
		// We successfully added tags to the geo, so we need to commit the changes
		if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiCommitGeo(OutNodeId))
			HOUDINI_LOG_WARNING(TEXT("Could not commit landscape spline control point geo!"));

		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		static constexpr bool bWaitForCompletion = false;
		if (!FHoudiniEngineUtils::HapiCookNode(OutNodeId, &CookOptions, bWaitForCompletion))
			return false;
	}

	return true;
}


void FindConnectedSplines(const TArray<TObjectPtr<ULandscapeSplineSegment>>& InSegments, TArray<FConnectedSpline>& OutConnectedSplines)
{
	TArray<TObjectPtr<ULandscapeSplineSegment>> SegmentsToProcess = InSegments;
	TSet<TObjectPtr<ULandscapeSplineSegment>> ProcessedSegments;
	FConnectedSpline* CurrentSpline = nullptr;
	while (SegmentsToProcess.Num() > 0)
	{
		const TObjectPtr<ULandscapeSplineSegment> Segment = SegmentsToProcess.Pop();
		if (ProcessedSegments.Contains(Segment))
			continue;

		if (!CurrentSpline)
		{
			CurrentSpline = &OutConnectedSplines.AddDefaulted_GetRef();
			FOrderedSegmentData SegmentData;
			SegmentData.Segment = Segment;
			CurrentSpline->OrderedSegments.Add(SegmentData);
			CurrentSpline->Start = Segment->Connections[0].ControlPoint;
			CurrentSpline->End = Segment->Connections[1].ControlPoint;

			ProcessedSegments.Add(Segment);
		}

		// Follow the chain of connected from CurrentSpline->Start to the end
		TObjectPtr<ULandscapeSplineSegment> LastSegment = Segment;
		int32 ConnectionIdx = 0;
		while (ConnectionIdx < CurrentSpline->Start->ConnectedSegments.Num())
		{
			const FLandscapeSplineConnection& Connection = CurrentSpline->Start->ConnectedSegments[ConnectionIdx];
			ConnectionIdx++;
			if (ProcessedSegments.Contains(Connection.Segment))
				continue;
			
			if (Connection.Segment->Connections[1].ControlPoint == CurrentSpline->Start)
			{
				CurrentSpline->Start = Connection.Segment->Connections[0].ControlPoint;
				ConnectionIdx = 0;
				FOrderedSegmentData SegmentData;
				SegmentData.Segment = Connection.Segment;
				CurrentSpline->OrderedSegments.Insert(SegmentData, 0);
				ProcessedSegments.Add(Connection.Segment);
			}
		}

		// Follow the chain of connected from CurrentSpline->End to the end 
		ConnectionIdx = 0;
		while (ConnectionIdx < CurrentSpline->End->ConnectedSegments.Num())
		{
			const FLandscapeSplineConnection& Connection = CurrentSpline->End->ConnectedSegments[ConnectionIdx];
			ConnectionIdx++;
			if (ProcessedSegments.Contains(Connection.Segment))
				continue;
			
			if (Connection.Segment->Connections[0].ControlPoint == CurrentSpline->End)
			{
				CurrentSpline->End = Connection.Segment->Connections[1].ControlPoint;
				ConnectionIdx = 0;
				FOrderedSegmentData SegmentData;
				SegmentData.Segment = Connection.Segment;
				CurrentSpline->OrderedSegments.Add(SegmentData);
				ProcessedSegments.Add(Connection.Segment);
			}
		}

		CurrentSpline = nullptr;
	}
}


void PopulateUnResampledPointData(
	TArray<FConnectedSpline>& InConnectedSplines,
	const EHoudiniUnrealLandscapeSplineCurve InExportCurve,
	const bool bInResampleSplines,
	const float InSplineResolution,
	int32& OutTotalNumPoints,
	TArray<int32>& OutPerSegmentVertexCount)
{
	int32 NextGlobalSegmentIndex = 0;
	int32 TotalNumPoints = 0;
	for (FConnectedSpline& ConnectedSpline : InConnectedSplines)
	{
		// Determine total number of points for all segments
		// Use helper structs to get keep the Center, Left and Right positions as well as the Alpha value and rotations
		// along the spline
		for (FOrderedSegmentData& SegmentData : ConnectedSpline.OrderedSegments)
		{
			SegmentData.GlobalSegmentIndex = NextGlobalSegmentIndex;
			NextGlobalSegmentIndex++;
			
			SegmentData.SegmentLength = 0.0f;
			if (!IsValid(SegmentData.Segment))
			{
				OutPerSegmentVertexCount.Add(0);
				continue;
			}

			// Calculate segment length and number of points per segment
			const TArray<FLandscapeSplineInterpPoint>& SegmentSplinePoints = SegmentData.Segment->GetPoints();
			const int32 NumPointsInSegment = SegmentSplinePoints.Num();

			// Initialize the unresampled points array and its first element
			if (NumPointsInSegment > 0)
			{
				SegmentData.UnResampledPoints.Reserve(NumPointsInSegment);
				SegmentData.UnResampledPoints.Emplace(InExportCurve, SegmentSplinePoints[0]);
			}
			// Populate the rest of the unresampled point array and calculate the rotations at each unresampled point
			for (int32 VertIdx = 1; VertIdx < NumPointsInSegment; ++VertIdx)
			{
				FHoudiniUnResampledPoint& Point0 = SegmentData.UnResampledPoints[VertIdx - 1];
				FHoudiniUnResampledPoint& Point1 = SegmentData.UnResampledPoints.Emplace_GetRef(
					InExportCurve, SegmentSplinePoints[VertIdx]);

				// Set rotations (first and last points use the control point's rotation)
				if (VertIdx == 1)
				{
					ULandscapeSplineControlPoint const* const CP = SegmentData.Segment->Connections[0].ControlPoint;
					if (IsValid(CP))
						Point0.Rotation = CP->Rotation.Quaternion();
					else
						Point0.Rotation = FQuat::Identity;
				}
				else if (VertIdx == NumPointsInSegment - 1)
				{
					Point0.CalculateRotationTo(Point1);
					ULandscapeSplineControlPoint const* const CP = SegmentData.Segment->Connections[1].ControlPoint;
					if (IsValid(CP))
						Point1.Rotation = CP->Rotation.Quaternion();
					else
						Point1.Rotation = FQuat::Identity;
				}
				else
				{
					Point0.CalculateRotationTo(Point1);
				}
			}

			int32 NumPointsInResampledSegment = 0;
			if (bInResampleSplines)
			{
				// Calculate the number of resampled points via SegmentLength / SplineResolution
				for (int32 VertIdx = 1; VertIdx < NumPointsInSegment; ++VertIdx)
				{
					const FHoudiniUnResampledPoint& Point0 = SegmentData.UnResampledPoints[VertIdx - 1];
					const FHoudiniUnResampledPoint& Point1 = SegmentData.UnResampledPoints[VertIdx];
					SegmentData.SegmentLength += (Point1.GetSelectedPosition() - Point0.GetSelectedPosition()).Length();
				}
				NumPointsInResampledSegment = FMath::CeilToInt32(SegmentData.SegmentLength / InSplineResolution) + 1; 
			}
			else
			{
				// Not resampling, so just use the points as is
				NumPointsInResampledSegment = NumPointsInSegment; 
			}

			// Record the number of (resampled) points we'll have in this spline/segment
			TotalNumPoints += NumPointsInResampledSegment;
			OutPerSegmentVertexCount.Add(NumPointsInResampledSegment);
		}
	}

	OutTotalNumPoints = TotalNumPoints;
}

bool FUnrealLandscapeSplineTranslator::ExtractSplineData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	FHoudiniUnrealLandscapeSplinesData& OutSplinesData,
	const EHoudiniUnrealLandscapeSplineCurve InExportCurve,
	float InSplineResolution)
{
	if (!IsValid(InSplinesComponent))
		return false;

	if (!InSplinesComponent->HasAnyControlPointsOrSegments())
		return false;

	// Use helper to fetch segments, since the Landscape Splines API differs between UE 5.0 and 5.1+
	TArray<TObjectPtr<ULandscapeSplineSegment>> Segments;
	if (!FHoudiniEngineRuntimeUtils::GetLandscapeSplinesSegments(InSplinesComponent, Segments))
		return false;

	// We need to determine which segments are connected with the same orientation. That way we can output a more
	// consistent / increasing point and vertex order per set of connected segments.
	TArray<FConnectedSpline> ConnectedSplines;
	FindConnectedSplines(Segments, ConnectedSplines);

	const int32 TotalNumSegments = Segments.Num();
	// Initialize arrays
	OutSplinesData.VertexCounts.Empty(TotalNumSegments);
	OutSplinesData.SegmentPaintLayerNames.Empty(TotalNumSegments);
	OutSplinesData.SegmentRaiseTerrains.Empty(TotalNumSegments);
	OutSplinesData.SegmentLowerTerrains.Empty(TotalNumSegments);

	// We only have to resample the splines if the spline resolution is different than the internal spline resolution
	// on the landscape splines component.
	const bool bResampleSplines = (InSplineResolution > 0.0f && InSplineResolution != InSplinesComponent->SplineResolution);
	int32 TotalNumPoints = 0;
	PopulateUnResampledPointData(ConnectedSplines, InExportCurve, bResampleSplines, InSplineResolution, TotalNumPoints, OutSplinesData.VertexCounts);

	OutSplinesData.PointPositions.Empty(TotalNumPoints);
	OutSplinesData.PointConnectionSocketNames.Empty(TotalNumPoints);
	OutSplinesData.PointConnectionTangentLengths.Empty(TotalNumPoints);
	OutSplinesData.ControlPointAttributes.Init(TotalNumPoints);

	// OutputPointIdx: The index of the current output point (across all segments). Range: [0, TotalNumPoints).
	//				   Incremented in the inner ResampledSegmentVertIdx loop.
	int32 OutputPointIdx = 0;
	for (const FConnectedSpline& ConnectedSpline : ConnectedSplines)
	{
		const int32 NumSegments = ConnectedSpline.OrderedSegments.Num();
		for (int32 SegmentIdx = 0; SegmentIdx < NumSegments; ++SegmentIdx)
		{
			const FOrderedSegmentData& SegmentData = ConnectedSpline.OrderedSegments[SegmentIdx];
			if (!IsValid(SegmentData.Segment))
			{
				// Create blank entries for this invalid segment
				OutSplinesData.SegmentPaintLayerNames.AddDefaulted();
				OutSplinesData.SegmentRaiseTerrains.AddDefaulted();
				OutSplinesData.SegmentLowerTerrains.AddDefaulted();

				continue;
			}

			const TArray<FLandscapeSplineInterpPoint>& SegmentPoints = SegmentData.Segment->GetPoints();
			const int32 NumVertsInSegment = SegmentPoints.Num();
			if (NumVertsInSegment <= 0)
			{
				// Create blank entries for this invalid segment
				OutSplinesData.SegmentPaintLayerNames.AddDefaulted();
				OutSplinesData.SegmentRaiseTerrains.AddDefaulted();
				OutSplinesData.SegmentLowerTerrains.AddDefaulted();

				continue;
			}

			// TODO: handle case NumVertsInSegment == 1
			int32 UnResampledPointIndex = 1;
			
			FHoudiniUnResampledPoint UnResampledPoint0 = SegmentData.UnResampledPoints[0];
			FHoudiniUnResampledPoint UnResampledPoint1 = SegmentData.UnResampledPoints[1];
			// If we are resampling, calculate the Alpha value [0, 1] along the segment, with Point 0 at Alpha = 0.
			if (bResampleSplines)
				UnResampledPoint1.Alpha = (UnResampledPoint1.GetSelectedPosition() - UnResampledPoint0.GetSelectedPosition()).Length() / SegmentData.SegmentLength;

			// Loop for the number of resampled points we'll have for this segment (which could be equal to original number
			// of points in segment if we are not resampling)
			const int32 NumResampledVertsInSegment = OutSplinesData.VertexCounts[SegmentData.GlobalSegmentIndex];
			for (int32 ResampledSegmentVertIdx = 0; ResampledSegmentVertIdx < NumResampledVertsInSegment; ++ResampledSegmentVertIdx, ++OutputPointIdx)
			{
				FVector ResampledPosition;
				FRotator ResampledRotation;

				float CalculatedHalfWidth = 0;
				float CalculatedSideFalloff = 0;
				if (bResampleSplines)
				{
					// Find P0 and P1: the unresampled points before and after the resampled point on the spline
					const float Alpha = static_cast<float>(ResampledSegmentVertIdx) / (NumResampledVertsInSegment - 1.0f);
					while (Alpha > UnResampledPoint1.Alpha && UnResampledPointIndex < NumVertsInSegment - 1)
					{
						UnResampledPoint0 = UnResampledPoint1;
						UnResampledPointIndex++;
						UnResampledPoint1 = SegmentData.UnResampledPoints[UnResampledPointIndex];
						UnResampledPoint1.Alpha = UnResampledPoint0.Alpha + (UnResampledPoint1.GetSelectedPosition() - UnResampledPoint0.GetSelectedPosition()).Length() / SegmentData.SegmentLength;
					}

					if (ResampledSegmentVertIdx == 0)
					{
						// The first point is a control point and always the same as the unresampled spline's first point
						ResampledPosition = UnResampledPoint0.GetSelectedPosition();
						ResampledRotation = UnResampledPoint0.Rotation.Rotator();
					}
					else if (ResampledSegmentVertIdx == NumResampledVertsInSegment - 1)
					{
						// The last point is a control point and always the same as the unresampled spline's last point
						ResampledPosition = UnResampledPoint1.GetSelectedPosition();
						ResampledRotation = UnResampledPoint1.Rotation.Rotator();
					}
					else
					{
						// Calculate the [0, 1] value representing the position of the resampled point between P0 and P1
						const float ResampleAlpha = (Alpha - UnResampledPoint0.Alpha) / (UnResampledPoint1.Alpha - UnResampledPoint0.Alpha);
						// Lerp to calculate the resampled point's position
						ResampledPosition = FMath::Lerp(
							UnResampledPoint0.GetSelectedPosition(), UnResampledPoint1.GetSelectedPosition(), ResampleAlpha);

						// Slerp to calculate the resampled point's rotation
						ResampledRotation = FQuat::Slerp( 
							UnResampledPoint0.Rotation, UnResampledPoint1.Rotation, ResampleAlpha).Rotator();

						// On points that are not control points, the half-width should be half the distance between the
						// Right and Left points going through the Center point
						const FVector ResampledLeft = FMath::Lerp(
						UnResampledPoint0.Left, UnResampledPoint1.Left, ResampleAlpha);
						const FVector ResampledRight = FMath::Lerp(
							UnResampledPoint0.Right, UnResampledPoint1.Right, ResampleAlpha);
						CalculatedHalfWidth = ((ResampledPosition - ResampledRight) + (ResampledLeft - ResampledPosition)).Length() / 2.0;

						const FVector ResampledLeftFalloff = FMath::Lerp(
							UnResampledPoint0.FalloffLeft, UnResampledPoint1.FalloffLeft, ResampleAlpha);
						const FVector ResampledRightFalloff = FMath::Lerp(
							UnResampledPoint0.FalloffRight, UnResampledPoint1.FalloffRight, ResampleAlpha);
						CalculatedSideFalloff = ((ResampledRightFalloff - ResampledRight).Length() 
							+ (ResampledLeftFalloff - ResampledLeft).Length()) / 2.0;
					}
				}
				else
				{
					// We are not resampling, so simply copy the unresampled position at this index
					UnResampledPointIndex = ResampledSegmentVertIdx;
					UnResampledPoint1 = SegmentData.UnResampledPoints[UnResampledPointIndex];
					ResampledPosition = UnResampledPoint1.GetSelectedPosition();
					ResampledRotation = UnResampledPoint1.Rotation.Rotator();

					if (ResampledSegmentVertIdx > 0 && ResampledSegmentVertIdx < NumResampledVertsInSegment - 1)
					{
						const FLandscapeSplineInterpPoint& SegmentPoint = SegmentPoints[UnResampledPointIndex];
						
						// On points that are not control points, the half-width should be half the distance between the
						// Right and Left points going through the Center point
						CalculatedHalfWidth = ((SegmentPoint.Center - SegmentPoint.Right) + (SegmentPoint.Left - SegmentPoint.Center)).Length() / 2.0;
						CalculatedSideFalloff = ((SegmentPoint.FalloffRight - SegmentPoint.Right).Length() 
							+ (SegmentPoint.FalloffLeft - SegmentPoint.Left).Length()) / 2.0;
					}
				}
				
				if (ResampledSegmentVertIdx == 0)
				{
					// First point is a control point, add the socket name
					static constexpr int32 ConnectionIdx = 0;
					OutSplinesData.PointConnectionSocketNames.Emplace(SegmentData.Segment->Connections[ConnectionIdx].SocketName.ToString());
					OutSplinesData.PointConnectionTangentLengths.Add(SegmentData.Segment->Connections[ConnectionIdx].TangentLen);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
					TObjectPtr<ULandscapeSplineControlPoint> CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
#else
					ULandscapeSplineControlPoint const* const CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
#endif
					if (!IsValid(CPoint))
					{
						OutSplinesData.ControlPointAttributes.AddEmpty();
					}
					else
					{
						OutSplinesData.ControlPointAttributes.AddControlPointData(
							CPoint, OutputPointIdx, InControlPointIdMap, InNextControlPointId);
					}
				}
				else if (ResampledSegmentVertIdx == NumResampledVertsInSegment - 1)
				{
					// Last point is a control point, add the socket name
					static constexpr int32 ConnectionIdx = 1;
					OutSplinesData.PointConnectionSocketNames.Emplace(SegmentData.Segment->Connections[ConnectionIdx].SocketName.ToString());
					OutSplinesData.PointConnectionTangentLengths.Add(SegmentData.Segment->Connections[ConnectionIdx].TangentLen);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
					TObjectPtr<ULandscapeSplineControlPoint> CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
#else
					ULandscapeSplineControlPoint const* const CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
#endif
					if (!IsValid(CPoint))
					{
						OutSplinesData.ControlPointAttributes.AddEmpty();
					}
					else
					{
						OutSplinesData.ControlPointAttributes.AddControlPointData(
							CPoint, OutputPointIdx, InControlPointIdMap, InNextControlPointId);
					}
				}
				else
				{
					// for other points the socket names, tangent lengths and control point name attributes are empty
					OutSplinesData.PointConnectionSocketNames.AddDefaulted();
					OutSplinesData.PointConnectionTangentLengths.AddDefaulted();
					OutSplinesData.ControlPointAttributes.AddEmpty();
					// The control point width was calculated, set that manually
					OutSplinesData.ControlPointAttributes.HalfWidths.Last() = CalculatedHalfWidth / HAPI_UNREAL_SCALE_FACTOR_POSITION;
					OutSplinesData.ControlPointAttributes.SideFalloffs.Last() = CalculatedSideFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION;
					// We don't have calculated end-falloff values for non-control points, set to 0
					OutSplinesData.ControlPointAttributes.EndFalloffs.Last() = 0.0f;
					// Set the calculated / resampled rotation
					ConvertAndSetRotation(
						ResampledRotation, OutSplinesData.ControlPointAttributes.Rotations.Num() - 4, OutSplinesData.ControlPointAttributes.Rotations);
				}

				// Set the final point position
				OutSplinesData.PointPositions.Add(ResampledPosition.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
				// Swap Y/Z
				OutSplinesData.PointPositions.Add(ResampledPosition.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
				OutSplinesData.PointPositions.Add(ResampledPosition.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);
			}
			
			// Extract general properties from the segment
			OutSplinesData.SegmentPaintLayerNames.Emplace(SegmentData.Segment->LayerName.ToString());
			OutSplinesData.SegmentRaiseTerrains.Add(SegmentData.Segment->bRaiseTerrain);
			OutSplinesData.SegmentLowerTerrains.Add(SegmentData.Segment->bLowerTerrain);

			// Extract the spline mesh configuration for the segment
			const int32 NumMeshes = SegmentData.Segment->SplineMeshes.Num();
			// Grow PerMeshSegmentData if needed
			if (OutSplinesData.PerMeshSegmentData.Num() < NumMeshes)
			{
				OutSplinesData.PerMeshSegmentData.SetNum(NumMeshes);
			}
			for (int32 MeshIdx = 0; MeshIdx < NumMeshes; ++MeshIdx)
			{
				const FLandscapeSplineMeshEntry& SplineMeshEntry = SegmentData.Segment->SplineMeshes[MeshIdx];
				FHoudiniUnrealLandscapeSplineSegmentMeshData& SegmentMeshData = OutSplinesData.PerMeshSegmentData[MeshIdx];
				// Initialize mesh per segment array if needed
				if (SegmentMeshData.MeshRefs.IsEmpty())
				{
					SegmentMeshData.MeshRefs.SetNum(TotalNumSegments);
				}

				// Set mesh reference (if there is a valid mesh for this entry) 
				if (IsValid(SplineMeshEntry.Mesh))
				{
					SegmentMeshData.MeshRefs[SegmentData.GlobalSegmentIndex] = SplineMeshEntry.Mesh->GetPathName();
				}

				// Material overrides: initialize the array to num material overrides
				const int32 NumMaterialOverrides = SplineMeshEntry.MaterialOverrides.Num();
				if (SegmentMeshData.MeshMaterialOverrideRefs.IsEmpty())
				{
					SegmentMeshData.MeshMaterialOverrideRefs.SetNum(NumMaterialOverrides);
				}

				// Set the material override refs
				for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
				{
					TArray<FString>& MaterialOverrideRefs = SegmentMeshData.MeshMaterialOverrideRefs[MaterialOverrideIdx];
					// Ensure there is enough space in the array for the segments 
					if (MaterialOverrideRefs.Num() < TotalNumSegments)
					{
						MaterialOverrideRefs.SetNum(TotalNumSegments);
					}
					
					UMaterialInterface const* const MaterialOverride = SplineMeshEntry.MaterialOverrides[MaterialOverrideIdx];
					if (!IsValid(MaterialOverride))
					{
						MaterialOverrideRefs[SegmentData.GlobalSegmentIndex] = TEXT("");
						continue;
					}

					MaterialOverrideRefs[SegmentData.GlobalSegmentIndex] = MaterialOverride->GetPathName();
				}
				
				// Initialize mesh scale per segment array if needed
				if (SegmentMeshData.MeshScales.IsEmpty())
				{
					SegmentMeshData.MeshScales.SetNum(TotalNumSegments * 3);
				}
				SegmentMeshData.MeshScales[SegmentData.GlobalSegmentIndex * 3 + 0] = SplineMeshEntry.Scale.X;
				SegmentMeshData.MeshScales[SegmentData.GlobalSegmentIndex * 3 + 1] = SplineMeshEntry.Scale.Z;
				SegmentMeshData.MeshScales[SegmentData.GlobalSegmentIndex * 3 + 2] = SplineMeshEntry.Scale.Y;
			}
		}
	}

	return true;
}


bool
FUnrealLandscapeSplineTranslator::ExtractSplineControlPointsData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	FHoudiniUnrealLandscapeSplinesControlPointData& OutSplinesControlPointData)
{
	if (!IsValid(InSplinesComponent))
		return false;

	if (!InSplinesComponent->HasAnyControlPointsOrSegments())
		return false;

	// Use helper to fetch control points since the landscape splines API differs between UE 5.0 and 5.1+
	TArray<TObjectPtr<ULandscapeSplineControlPoint>> ControlPoints;
	if (!FHoudiniEngineRuntimeUtils::GetLandscapeSplinesControlPoints(InSplinesComponent, ControlPoints))
		return false;
	
	// Initialize control point arrays
	const int32 NumControlPoints = ControlPoints.Num();
	OutSplinesControlPointData.ControlPointPositions.Empty(NumControlPoints * 3);
	OutSplinesControlPointData.Attributes.Init(NumControlPoints);
	
	for (int32 ControlPointIdx = 0; ControlPointIdx < NumControlPoints; ++ControlPointIdx)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		TObjectPtr<ULandscapeSplineControlPoint> CPoint = ControlPoints[ControlPointIdx];
#else
		ULandscapeSplineControlPoint const* const CPoint = ControlPoints[ControlPointIdx];
#endif
		if (!IsValid(CPoint))
			continue;

		// Convert the position and rotation values to Houdini's coordinate system and scale 
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);

		OutSplinesControlPointData.Attributes.AddControlPointData(
			CPoint, ControlPointIdx, InControlPointIdMap, InNextControlPointId);
	}

	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddTargetLandscapeAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	ALandscapeProxy const* const InLandscapeActor,
	int InCount,
	HAPI_AttributeOwner InAttribOwner)
{
	if (!IsValid(InLandscapeActor))
		return false;

	// Extract the actor path
	const FString LandscapeActorPath = InLandscapeActor->GetPathName();

	// Set the attribute's string data
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE);
	Accessor.AddAttribute(InAttribOwner, HAPI_STORAGETYPE_STRING, 1, InCount, &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttrInfo, LandscapeActorPath), false);

	return true;
}	


bool
FUnrealLandscapeSplineTranslator::AddOutputAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	int InValue,
	int InCount,
	HAPI_AttributeOwner InAttribOwner)
{
	// Set the attribute's string data
	TArray<int32> LandscapeSplineOutput;
	LandscapeSplineOutput.Init(InValue, InCount);

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE);
	Accessor.AddAttribute(InAttribOwner, HAPI_STORAGETYPE_INT, 1, InCount, &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, LandscapeSplineOutput), false);

	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddPositionAttribute(HAPI_NodeId InNodeId, const TArray<float>& InPositions)
{
	if (InPositions.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, InPositions.Num() / 3, &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InPositions), false);

	return true;	
}


bool
FUnrealLandscapeSplineTranslator::AddPaintLayerNameAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InData, HAPI_AttributeOwner InAttribOwner)
{
	if (InData.IsEmpty())
		return false;
	
	const char * AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_PAINT_LAYER_NAME
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, AttributeName);
	Accessor.AddAttribute(InAttribOwner, HAPI_STORAGETYPE_STRING, 1, InData.Num(), &AttrInfo);
	bool bSuccess = Accessor.SetAttributeData(AttrInfo, InData);

	return bSuccess;
}


bool FUnrealLandscapeSplineTranslator::AddRaiseTerrainAttribute(HAPI_NodeId InNodeId, const TArray<int8>& InData, HAPI_AttributeOwner InAttribOwner)
{
	if (InData.IsEmpty())
		return false;

	const FString AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_RAISE_TERRAIN
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	Accessor.AddAttribute(InAttribOwner, HAPI_STORAGETYPE_INT8, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);

	return true;
}


bool FUnrealLandscapeSplineTranslator::AddLowerTerrainAttribute(HAPI_NodeId InNodeId, const TArray<int8>& InData, HAPI_AttributeOwner InAttribOwner)
{
	if (InData.IsEmpty())
		return false;


	const FString AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_LOWER_TERRAIN
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	Accessor.AddAttribute(InAttribOwner, HAPI_STORAGETYPE_INT8, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);

	return true;
}


bool FUnrealLandscapeSplineTranslator::AddSegmentMeshesAttributes(HAPI_NodeId InNodeId, const TArray<FHoudiniUnrealLandscapeSplineSegmentMeshData>& InPerMeshSegmentData)
{
	int32 NumMeshAttrs = InPerMeshSegmentData.Num();
	if (NumMeshAttrs <= 0)
		return false;

	const int32 NumSegments = InPerMeshSegmentData[0].MeshRefs.Num();

	bool bNeedToCommit = false;
	for (int32 MeshIdx = 0; MeshIdx < NumMeshAttrs; ++MeshIdx)
	{
		const FHoudiniUnrealLandscapeSplineSegmentMeshData& MeshSegmentData = InPerMeshSegmentData[MeshIdx];
		HAPI_AttributeInfo AttrInfo;

		// Add the mesh attribute
		FString MeshAttrName = (MeshIdx == 0 ? FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH)) : FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH "%d"), MeshIdx));
		FHoudiniHapiAccessor MeshAttrAccessor(InNodeId, 0, TCHAR_TO_ANSI(*MeshAttrName));
		MeshAttrAccessor.AddAttribute(HAPI_ATTROWNER_PRIM, HAPI_STORAGETYPE_STRING, 1, NumSegments, &AttrInfo);
		MeshAttrAccessor.SetAttributeData(AttrInfo, MeshSegmentData.MeshRefs);

		// Add the mesh scale attribute
		FString MeshScaleAttrName = FString::Printf(TEXT("%s%s"), *MeshAttrName, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX));
		FHoudiniHapiAccessor ScaleAttrAccessor(InNodeId, 0, TCHAR_TO_ANSI(*MeshScaleAttrName));
		ScaleAttrAccessor.AddAttribute(HAPI_ATTROWNER_PRIM, HAPI_STORAGETYPE_FLOAT, 3, NumSegments, &AttrInfo);
		bool bSuccess = ScaleAttrAccessor.SetAttributeData(AttrInfo, MeshSegmentData.MeshScales);

		// Material overrides
		int NumMaterialOverrides = MeshSegmentData.MeshMaterialOverrideRefs.Num();
		FString MaterialOverrideAttrNamePrefix = FString::Printf(TEXT("%s%s"), *MeshAttrName, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX));
		for (int MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
		{
			FString MaterialOverrideAttrName = (MaterialOverrideIdx == 0? MaterialOverrideAttrNamePrefix : FString::Printf(TEXT("%s%d"), *MaterialOverrideAttrNamePrefix, MaterialOverrideIdx));
			FHoudiniHapiAccessor MaterialAttrAccessor(InNodeId, 0, TCHAR_TO_ANSI(*MaterialOverrideAttrName));
			MaterialAttrAccessor.AddAttribute(HAPI_ATTROWNER_PRIM, HAPI_STORAGETYPE_STRING, 1, NumSegments, &AttrInfo);
			MaterialAttrAccessor.SetAttributeData(AttrInfo, MeshSegmentData.MeshMaterialOverrideRefs[MaterialOverrideIdx]);
		}
	}
	
	return bNeedToCommit;
}


bool
FUnrealLandscapeSplineTranslator::AddConnectionSocketNameAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SOCKET_NAME);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_STRING, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddRotationAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 4, InData.Num() / 4, &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;

}


bool
FUnrealLandscapeSplineTranslator::AddMeshAttribute(HAPI_NodeId InNodeId, const TArray<FString>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_STRING, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddMaterialOverrideAttributes(HAPI_NodeId InNodeId, const TArray<TArray<FString>>& InMaterialOverrideRefs)
{
	// The InMaterialOverrideRefs[0] array contains the values for unreal_landscape_spline_mesh_material_override for all control points
// InPerMeshSegmentData[1] array contains the values for unreal_landscape_spline_mesh_material_override1 for all control points etc

	if (InNodeId < 0)
		return false;

	const int NumMaterialOverrides = InMaterialOverrideRefs.Num();
	if (NumMaterialOverrides <= 0)
		return false;

	int NumControlPoints = InMaterialOverrideRefs[0].Num();
	bool bNeedToCommit = false;
	const FString AttrNamePrefix(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX));
	for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
	{
		// Add the material override attribute
		const FString MaterialOverrideAttrName = (MaterialOverrideIdx == 0)
			? AttrNamePrefix 
			: FString::Printf(TEXT("%s%d"), *AttrNamePrefix, MaterialOverrideIdx);

		HAPI_AttributeInfo AttrInfo;
		FHoudiniHapiAccessor Accessor(InNodeId, 0, TCHAR_TO_ANSI(*MaterialOverrideAttrName));
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_STRING, 1, NumControlPoints, &AttrInfo);
		Accessor.SetAttributeData(AttrInfo, InMaterialOverrideRefs[MaterialOverrideIdx]);
	}
	
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddIdsAttribute(HAPI_NodeId InNodeId, const TArray<int32>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_ID);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_INT, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddHalfWidthAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddSideFalloffAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SIDE_FALLOFF);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddEndFalloffAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_END_FALLOFF);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddTangentLengthAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TANGENT_LENGTH);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, InData.Num(), &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);
	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddMeshScaleAttribute(HAPI_NodeId InNodeId, const TArray<float>& InData)
{
	if (InData.IsEmpty())
		return false;
	
	const char * AttrName = HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InNodeId, 0, AttrName);
	Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, InData.Num() / 3, &AttrInfo);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttrInfo, InData), false);

	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddControlPointAttributes(HAPI_NodeId InNodeId, const FHoudiniUnrealLandscapeSplineControlPointAttributes& InControlPointAttributes)
{
	bool bNeedToCommit = false;

	if (AddRotationAttribute(InNodeId, InControlPointAttributes.Rotations))
		bNeedToCommit = true;
	
	if (AddIdsAttribute(InNodeId, InControlPointAttributes.Ids))
		bNeedToCommit = true;

	if (AddHalfWidthAttribute(InNodeId, InControlPointAttributes.HalfWidths))
		bNeedToCommit = true;

	if (AddSideFalloffAttribute(InNodeId, InControlPointAttributes.SideFalloffs))
		bNeedToCommit = true;

	if (AddEndFalloffAttribute(InNodeId, InControlPointAttributes.EndFalloffs))
		bNeedToCommit = true;

	if (AddPaintLayerNameAttribute(InNodeId, InControlPointAttributes.PaintLayerNames, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;

	if (AddRaiseTerrainAttribute(InNodeId, InControlPointAttributes.RaiseTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLowerTerrainAttribute(InNodeId, InControlPointAttributes.LowerTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;

	if (AddMeshAttribute(InNodeId, InControlPointAttributes.MeshRefs))
		bNeedToCommit = true;
	
	if (AddMaterialOverrideAttributes(InNodeId, InControlPointAttributes.MaterialOverrideRefs))
		bNeedToCommit = true;

	if (AddMeshScaleAttribute(InNodeId, InControlPointAttributes.MeshScales))
		bNeedToCommit = true;
	return bNeedToCommit;
}
