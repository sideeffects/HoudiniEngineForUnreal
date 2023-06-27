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
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"

#include "Landscape.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "Materials/MaterialInterface.h"


bool
FUnrealLandscapeSplineTranslator::CreateInputNodeForLandscapeSplinesComponent(
	ULandscapeSplinesComponent* const InSplinesComponent, 
	HAPI_NodeId& OutCreatedInputNodeId,
	FUnrealObjectInputHandle& OutInputNodeHandle,
	const FString& InNodeName,
	const bool bInExportCurves,
	const bool bInExportControlPoints,
	const bool bInExportLeftRightCurves,
	const bool bInInputNodesCanBeDeleted)
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
	// const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	// TODO: use the ref counted system after adding supporting for identifiers for leaf nodes that use custom paths
	static constexpr bool bUseRefCountedInputSystem = false;
	if (bUseRefCountedInputSystem)
	{
		// Check if we already have an input node for this component and its options
		bool bSingleLeafNodeOnly = false;
		FUnrealObjectInputIdentifier IdentReferenceNode;
		TArray<FUnrealObjectInputIdentifier> IdentPerOption;
		if (!FHoudiniEngineUtils::BuildLandscapeSplinesInputObjectIdentifiers(
			InSplinesComponent,
			bInExportCurves,
			bInExportControlPoints,
			bInExportLeftRightCurves,
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
		if (FHoudiniEngineUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FHoudiniEngineUtils::GetHAPINodeId(Handle, NodeId) && (bSingleLeafNodeOnly || FHoudiniEngineUtils::AreReferencedHAPINodesValid(Handle)))
			{
				if (!bInInputNodesCanBeDeleted)
				{
					// Make sure to prevent deletion of the input node if needed
					FHoudiniEngineUtils::UpdateInputNodeCanBeDeleted(Handle, bInInputNodesCanBeDeleted);
				}

				OutInputNodeHandle = Handle;
				OutCreatedInputNodeId = NodeId;
				return true;
			}
		}

		FHoudiniEngineUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FHoudiniEngineUtils::EnsureParentsExist(Identifier, ParentHandle, bInInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FHoudiniEngineUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

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
				FHoudiniEngineUtils::GetDefaultInputNodeName(OptionIdentifier, NodeLabel);
				
				if (!CreateInputNodeForLandscapeSplinesComponent(
						InSplinesComponent,
						NewNodeId,
						OptionHandle,
						NodeLabel,
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
			if (!FHoudiniEngineUtils::CreateOrUpdateReferenceInputMergeNode(IdentReferenceNode, PerOptionNodeHandles, RefNodeHandle, true, bInInputNodesCanBeDeleted))
				return false;
			
			OutInputNodeHandle = RefNodeHandle;
			FHoudiniEngineUtils::GetHAPINodeId(IdentReferenceNode, OutCreatedInputNodeId);
			return true;
		}
	}

	HAPI_NodeId PreviousInputNodeId = OutCreatedInputNodeId;

	const bool bUseMergeNode = (bInExportCurves && bInExportControlPoints) || bInExportLeftRightCurves;
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
	
	// We have now created a valid new input node, delete the previous one
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *FinalInputNodeName);
		}
	}

	bool bSuccess = true;
	int32 MergeNodeInputIdx = 0;
	if (bInExportCurves)
	{
		HAPI_NodeId SplinesNodeId = -1;
		if (!CreateInputNodeForLandscapeSplines(
				InSplinesComponent, ObjectNodeId, FinalInputNodeName, SplinesNodeId,
				EHoudiniLandscapeSplineCurve::Center))
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
		if (!CreateInputNodeForLandscapeSplinesControlPoints(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, ControlPointCloudNodeId))
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
		if (!CreateInputNodeForLandscapeSplines(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, SplinesNodeId,
			EHoudiniLandscapeSplineCurve::Left))
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
		if (!CreateInputNodeForLandscapeSplines(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, SplinesNodeId,
			EHoudiniLandscapeSplineCurve::Right))
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

	if (bUseRefCountedInputSystem)
	{
		// Get our parent OBJ NodeID
		const HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(OutCreatedInputNodeId);
		static constexpr TSet<FUnrealObjectInputHandle> const* ReferencedNodes = nullptr; 
		FUnrealObjectInputHandle Handle;
		if (FHoudiniEngineUtils::AddNodeOrUpdateNode(Identifier, OutCreatedInputNodeId, Handle, InputObjectNodeId, ReferencedNodes, bInInputNodesCanBeDeleted))
			OutInputNodeHandle = Handle;
	}

	return bSuccess;
}

bool
FUnrealLandscapeSplineTranslator::CreateInputNodeForLandscapeSplines(
	ULandscapeSplinesComponent* const InSplinesComponent,
	const HAPI_NodeId& InObjectNodeId,
	const FString& InNodeName,
	HAPI_NodeId& OutNodeId,
	const EHoudiniLandscapeSplineCurve InExportCurve)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Input node name: InNodeName + a suffix describing if this is center or left/right curves.
	FString FinalInputNodeName;
	switch (InExportCurve)
	{
	case EHoudiniLandscapeSplineCurve::Center:
		FinalInputNodeName = InNodeName + TEXT("_curves");
		break;
	case EHoudiniLandscapeSplineCurve::Left:
		FinalInputNodeName = InNodeName + TEXT("_left_curves");
		break;
	case EHoudiniLandscapeSplineCurve::Right:
		FinalInputNodeName = InNodeName + TEXT("_right_curves");
		break;
	case EHoudiniLandscapeSplineCurve::Invalid:
	default:
		HOUDINI_LOG_WARNING(TEXT("Unsupported value %d for InExportCurve, aborting CreateInputNodeForLandscapeSplines."), InExportCurve);
		return false;
	}

	FLandscapeSplinesData SplinesData;
	if (!ExtractLandscapeSplineData(InSplinesComponent, SplinesData, InExportCurve))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to extract landscape splines data."));
		return false;
	}

	// Extract linear position array and calculate number of vertices
	const int32 NumSegments = SplinesData.VertexCounts.Num();
	const int32 NumVerts = SplinesData.PointPositions.Num() / 3;
	
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();

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
	if (AddLandscapeSplinePositionAttribute(OutNodeId, SplinesData.PointPositions))
		bNeedToCommit = true;

	if (AddLandscapeSplineControlPointNamesAttribute(OutNodeId, SplinesData.ControlPointNames))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineHalfWidthAttribute(OutNodeId, SplinesData.ControlPointHalfWidths))
		bNeedToCommit = true;

	if (AddLandscapeSplineTangentLengthAttribute(OutNodeId, SplinesData.ControlPointTangentLengths))
		bNeedToCommit = true;

	// Segment attributes
	if (AddLandscapeSplinePaintLayerNameAttribute(OutNodeId, SplinesData.SegmentPaintLayerNames, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineRaiseTerrainAttribute(OutNodeId, SplinesData.SegmentRaiseTerrains, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineLowerTerrainAttribute(OutNodeId, SplinesData.SegmentLowerTerrains, HAPI_ATTROWNER_PRIM))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineSegmentMeshesAttributes(OutNodeId, SplinesData.PerMeshSegmentData))
		bNeedToCommit = true;

	// Segment connection attributes
	if (AddLandscapeSplineConnectionSocketNameAttribute(OutNodeId, SplinesData.PointConnectionSocketNames))
		bNeedToCommit = true;

	// Add the unreal_landscape_spline_output attribute to indicate that this a landscape spline and not a normal curve
	if (AddLandscapeSplineOutputAttribute(OutNodeId, 0, static_cast<int32>(InExportCurve), PartInfo.faceCount))
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
				if (AddLandscapeSplineTargetLandscapeAttribute(OutNodeId, 0, LandscapeInfo->LandscapeActor.Get(), PartInfo.faceCount))
					bNeedToCommit = true;
			}
		}
	}

	if (bNeedToCommit) 
	{
		// We successfully added tags to the geo, so we need to commit the changes
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), OutNodeId))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups for the landscape spline input's tags!"));

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
FUnrealLandscapeSplineTranslator::CreateInputNodeForLandscapeSplinesControlPoints(
	ULandscapeSplinesComponent* const InSplinesComponent,
	const HAPI_NodeId& InObjectNodeId,
	const FString& InNodeName,
	HAPI_NodeId& OutNodeId)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Set the final node name with _control_points suffix
	const FString FinalInputNodeName = InNodeName + TEXT("_control_points");

	FLandscapeSplinesControlPointData ControlPointsData;
	if (!ExtractLandscapeSplineControlPointData(InSplinesComponent, ControlPointsData))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to extract landscape splines control points data."));
		return false;
	}

	const int32 NumPoints = ControlPointsData.ControlPointPositions.Num() / 3;
	
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();

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

	if (AddLandscapeSplinePositionAttribute(OutNodeId, ControlPointsData.ControlPointPositions))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointRotationAttribute(OutNodeId, ControlPointsData.ControlPointRotations))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointNamesAttribute(OutNodeId, ControlPointsData.ControlPointNames))
		bNeedToCommit = true;

	if (AddLandscapeSplineHalfWidthAttribute(OutNodeId, ControlPointsData.ControlPointHalfWidths))
		bNeedToCommit = true;

	if (AddLandscapeSplinePaintLayerNameAttribute(OutNodeId, ControlPointsData.ControlPointPaintLayerNames, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineRaiseTerrainAttribute(OutNodeId, ControlPointsData.ControlPointRaiseTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineLowerTerrainAttribute(OutNodeId, ControlPointsData.ControlPointLowerTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointMeshAttribute(OutNodeId, ControlPointsData.ControlPointMeshRefs))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointMaterialOverrideAttributes(OutNodeId, ControlPointsData.PerMaterialOverrideControlPointRefs))
		bNeedToCommit = true;

	if (AddLandscapeSplineControlPointMeshScaleAttribute(OutNodeId, ControlPointsData.ControlPointMeshScales))
		bNeedToCommit = true;

	// Add the unreal_landscape_spline_output attribute to indicate that this a landscape spline and not a normal curve
	if (AddLandscapeSplineOutputAttribute(OutNodeId, 0, PartInfo.pointCount, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT))
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
				if (AddLandscapeSplineTargetLandscapeAttribute(OutNodeId, 0, LandscapeInfo->LandscapeActor.Get(), PartInfo.pointCount, HAPI_ATTROWNER_POINT))
					bNeedToCommit = true;
			}
		}
	}

	if (bNeedToCommit) 
	{
		// We successfully added tags to the geo, so we need to commit the changes
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), OutNodeId))
			HOUDINI_LOG_WARNING(TEXT("Could not commit landscape spline control point geo!"));

		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		static constexpr bool bWaitForCompletion = false;
		if (!FHoudiniEngineUtils::HapiCookNode(OutNodeId, &CookOptions, bWaitForCompletion))
			return false;
	}

	return true;
}

bool FUnrealLandscapeSplineTranslator::ExtractLandscapeSplineData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	FLandscapeSplinesData& OutSplinesData,
	const EHoudiniLandscapeSplineCurve InExportCurve)
{
	if (!IsValid(InSplinesComponent))
		return false;

	if (!InSplinesComponent->HasAnyControlPointsOrSegments())
		return false;

	// Use helper to fetch segments, since the Landscape Splines API differs between UE 5.0 and 5.1+
	TArray<TObjectPtr<ULandscapeSplineSegment>> Segments;
	if (!FHoudiniEngineRuntimeUtils::GetLandscapeSplinesSegments(InSplinesComponent, Segments))
		return false;

	const int32 NumSegments = Segments.Num();
	// Initialize arrays
	OutSplinesData.VertexCounts.Empty(NumSegments);
	OutSplinesData.SegmentPaintLayerNames.Empty(NumSegments);
	OutSplinesData.SegmentRaiseTerrains.Empty(NumSegments);
	OutSplinesData.SegmentLowerTerrains.Empty(NumSegments);

	// Determine total number of points for all segments
	int32 NumPoints = 0;
	for (int32 SegmentIdx = 0; SegmentIdx < NumSegments; ++SegmentIdx)
	{
		ULandscapeSplineSegment const* const Segment = Segments[SegmentIdx];
		if (!IsValid(Segment))
			continue;
		NumPoints += Segment->GetPoints().Num();
	}
	OutSplinesData.PointPositions.Empty(NumPoints);
	OutSplinesData.PointConnectionSocketNames.Empty(NumPoints);
	OutSplinesData.ControlPointTangentLengths.Empty(NumPoints);
	OutSplinesData.ControlPointNames.Empty(NumPoints);
	OutSplinesData.ControlPointHalfWidths.Empty(NumPoints);

	for (int32 SegmentIdx = 0; SegmentIdx < NumSegments; ++SegmentIdx)
	{
		ULandscapeSplineSegment const* const Segment = Segments[SegmentIdx];
		if (!IsValid(Segment))
		{
			// Create blank entries for this invalid segment
			OutSplinesData.SegmentPaintLayerNames.AddDefaulted();
			OutSplinesData.SegmentRaiseTerrains.AddDefaulted();
			OutSplinesData.SegmentLowerTerrains.AddDefaulted();
			OutSplinesData.VertexCounts.Add(0);

			continue;
		}

		const TArray<FLandscapeSplineInterpPoint>& SegmentPoints = Segment->GetPoints();
		const int32 NumVertsInSegment = SegmentPoints.Num();
		if (NumVertsInSegment <= 0)
		{
			// Create blank entries for this invalid segment
			OutSplinesData.SegmentPaintLayerNames.AddDefaulted();
			OutSplinesData.SegmentRaiseTerrains.AddDefaulted();
			OutSplinesData.SegmentLowerTerrains.AddDefaulted();
			OutSplinesData.VertexCounts.Add(0);

			continue;
		}

		for (int32 SegmentVertIdx = 0; SegmentVertIdx < NumVertsInSegment; ++SegmentVertIdx)
		{
			const FLandscapeSplineInterpPoint& SegmentPoint = SegmentPoints[SegmentVertIdx];

			// Get the point to use (center, left or right)
			FVector CurvePoint;
			switch (InExportCurve)
			{
			case EHoudiniLandscapeSplineCurve::Center:
				CurvePoint = SegmentPoint.Center;
				break;
			case EHoudiniLandscapeSplineCurve::Left:
				CurvePoint = SegmentPoint.Left;
				break;
			case EHoudiniLandscapeSplineCurve::Right:
				CurvePoint = SegmentPoint.Right;
				break;
			default:
				HOUDINI_LOG_WARNING(TEXT("Invalid value for InExportCurve: %d, aborting landscape spline data extraction."), InExportCurve);
				return false;
			}
			
			OutSplinesData.PointPositions.Add(CurvePoint.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
			// Swap Y/Z
			OutSplinesData.PointPositions.Add(CurvePoint.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
			OutSplinesData.PointPositions.Add(CurvePoint.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);

			if (SegmentVertIdx == 0)
			{
				// First point is a control point, add the socket name
				OutSplinesData.PointConnectionSocketNames.Emplace(Segment->Connections[0].SocketName.ToString());
				OutSplinesData.ControlPointTangentLengths.Add(Segment->Connections[0].TangentLen);
				ULandscapeSplineControlPoint const* const CPoint = Segment->Connections[0].ControlPoint;
				if (!IsValid(CPoint))
				{
					OutSplinesData.ControlPointNames.AddDefaulted();
					OutSplinesData.ControlPointHalfWidths.AddDefaulted();
				}
				else
				{
					OutSplinesData.ControlPointNames.Emplace(CPoint->GetName());
					OutSplinesData.ControlPointHalfWidths.Emplace(CPoint->Width);
				}
			}
			else if (SegmentVertIdx == NumVertsInSegment - 1)
			{
				// Last point is a control point, add the socket name
				OutSplinesData.PointConnectionSocketNames.Emplace(Segment->Connections[1].SocketName.ToString());
				OutSplinesData.ControlPointTangentLengths.Add(Segment->Connections[1].TangentLen);
				ULandscapeSplineControlPoint const* const CPoint = Segment->Connections[1].ControlPoint;
				if (!IsValid(CPoint))
				{
					OutSplinesData.ControlPointNames.AddDefaulted();
					OutSplinesData.ControlPointHalfWidths.AddDefaulted();
				}
				else
				{
					OutSplinesData.ControlPointNames.Emplace(CPoint->GetName());
					OutSplinesData.ControlPointHalfWidths.Emplace(CPoint->Width);
				}
			}
			else
			{
				OutSplinesData.PointConnectionSocketNames.AddDefaulted();
				OutSplinesData.ControlPointTangentLengths.AddDefaulted();
				OutSplinesData.ControlPointNames.AddDefaulted();

				// On points that are not control points, the half-width should be half the distance between the
				// Right and Left points going through the Center point
				OutSplinesData.ControlPointHalfWidths.Add(
					((SegmentPoint.Center - SegmentPoint.Right) + (SegmentPoint.Left - SegmentPoint.Center)).Length() / 2.0);
			}
		}
		
		OutSplinesData.VertexCounts.Add(NumVertsInSegment);

		// Extract general properties from the segment
		OutSplinesData.SegmentPaintLayerNames.Emplace(Segment->LayerName.ToString());
		OutSplinesData.SegmentRaiseTerrains.Add(Segment->bRaiseTerrain);
		OutSplinesData.SegmentLowerTerrains.Add(Segment->bLowerTerrain);

		// Extract the spline mesh configuration for the segment
		const int32 NumMeshes = Segment->SplineMeshes.Num();
		// Grow PerMeshSegmentData if needed
		if (OutSplinesData.PerMeshSegmentData.Num() < NumMeshes)
		{
			OutSplinesData.PerMeshSegmentData.SetNum(NumMeshes);
		}
		for (int32 MeshIdx = 0; MeshIdx < NumMeshes; ++MeshIdx)
		{
			const FLandscapeSplineMeshEntry& SplineMeshEntry = Segment->SplineMeshes[MeshIdx];
			FLandscapeSplineSegmentMeshData& SegmentMeshData = OutSplinesData.PerMeshSegmentData[MeshIdx];
			// Initialize mesh per segment array if needed
			if (SegmentMeshData.MeshRefs.IsEmpty())
			{
				SegmentMeshData.MeshRefs.SetNum(NumSegments);
			}

			// Set mesh reference (if there is a valid mesh for this entry) 
			if (IsValid(SplineMeshEntry.Mesh))
			{
				SegmentMeshData.MeshRefs[SegmentIdx] = SplineMeshEntry.Mesh->GetPathName();
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
				if (MaterialOverrideRefs.Num() < NumSegments)
				{
					MaterialOverrideRefs.SetNum(NumSegments);
				}
				
				UMaterialInterface const* const MaterialOverride = SplineMeshEntry.MaterialOverrides[MaterialOverrideIdx];
				if (!IsValid(MaterialOverride))
				{
					MaterialOverrideRefs[SegmentIdx] = TEXT("");
					continue;
				}

				MaterialOverrideRefs[SegmentIdx] = MaterialOverride->GetPathName();
			}
			
			// Initialize mesh scale per segment array if needed
			if (SegmentMeshData.MeshScales.IsEmpty())
			{
				SegmentMeshData.MeshScales.SetNum(NumSegments * 3);
			}
			SegmentMeshData.MeshScales[SegmentIdx * 3 + 0] = SplineMeshEntry.Scale.X;
			SegmentMeshData.MeshScales[SegmentIdx * 3 + 1] = SplineMeshEntry.Scale.Z;
			SegmentMeshData.MeshScales[SegmentIdx * 3 + 2] = SplineMeshEntry.Scale.Y;
		}
	}

	return true;
}

bool
FUnrealLandscapeSplineTranslator::ExtractLandscapeSplineControlPointData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	FLandscapeSplinesControlPointData& OutSplinesControlPointData)
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
	OutSplinesControlPointData.ControlPointRotations.Empty(NumControlPoints * 4);
	OutSplinesControlPointData.ControlPointPaintLayerNames.Empty(NumControlPoints);
	OutSplinesControlPointData.ControlPointRaiseTerrains.Empty(NumControlPoints);
	OutSplinesControlPointData.ControlPointLowerTerrains.Empty(NumControlPoints);
	OutSplinesControlPointData.ControlPointMeshRefs.Empty(NumControlPoints);
	OutSplinesControlPointData.ControlPointMeshScales.Empty(NumControlPoints * 3);
	OutSplinesControlPointData.ControlPointNames.Empty(NumControlPoints);
	OutSplinesControlPointData.ControlPointHalfWidths.Empty(NumControlPoints);
	
	for (int32 ControlPointIdx = 0; ControlPointIdx < NumControlPoints; ++ControlPointIdx)
	{
		ULandscapeSplineControlPoint const* const CPoint = ControlPoints[ControlPointIdx];
		if (!IsValid(CPoint))
			continue;

		// Convert the position and rotation values to Houdini's coordinate system and scale 
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);		
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);		
		OutSplinesControlPointData.ControlPointPositions.Add(CPoint->Location.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);		

		const FQuat CPRot(CPoint->Rotation);
		OutSplinesControlPointData.ControlPointRotations.Add(CPRot.X);
		OutSplinesControlPointData.ControlPointRotations.Add(CPRot.Z);
		OutSplinesControlPointData.ControlPointRotations.Add(CPRot.Y);
		OutSplinesControlPointData.ControlPointRotations.Add(-CPRot.W);

		OutSplinesControlPointData.ControlPointNames.Emplace(CPoint->GetName());

		OutSplinesControlPointData.ControlPointHalfWidths.Add(CPoint->Width);

		OutSplinesControlPointData.ControlPointPaintLayerNames.Emplace(CPoint->LayerName.ToString());
		OutSplinesControlPointData.ControlPointRaiseTerrains.Add(CPoint->bRaiseTerrain);
		OutSplinesControlPointData.ControlPointLowerTerrains.Add(CPoint->bLowerTerrain);

		// Set the static mesh reference 
		OutSplinesControlPointData.ControlPointMeshRefs.Emplace(IsValid(CPoint->Mesh) ? CPoint->Mesh->GetPathName() : FString());

		const int32 NumMaterialOverrides = CPoint->MaterialOverrides.Num();
		if (OutSplinesControlPointData.PerMaterialOverrideControlPointRefs.Num() < NumMaterialOverrides)
			OutSplinesControlPointData.PerMaterialOverrideControlPointRefs.SetNum(NumMaterialOverrides);
		for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
		{
			UMaterialInterface const* const Material = CPoint->MaterialOverrides[MaterialOverrideIdx];
			// Initialize the per control point array for this override index if necessary
			TArray<FString>& PerCPMaterialOverrideRefs = OutSplinesControlPointData.PerMaterialOverrideControlPointRefs[MaterialOverrideIdx];
			if (PerCPMaterialOverrideRefs.IsEmpty())
				PerCPMaterialOverrideRefs.SetNum(NumControlPoints);

			// Set the material ref or empty string if the material is invalid
			PerCPMaterialOverrideRefs[ControlPointIdx] = IsValid(Material) ? Material->GetPathName() : FString(); 
		}

		OutSplinesControlPointData.ControlPointMeshScales.Add(CPoint->MeshScale.X);
		OutSplinesControlPointData.ControlPointMeshScales.Add(CPoint->MeshScale.Z);
		OutSplinesControlPointData.ControlPointMeshScales.Add(CPoint->MeshScale.Y);
	}

	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineTargetLandscapeAttribute(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	ALandscapeProxy const* const InLandscapeActor,
	const int32 InCount,
	const HAPI_AttributeOwner InAttribOwner)
{
	if (InNodeId < 0 || InCount <= 0)
		return false;

	if (!IsValid(InLandscapeActor))
		return false;

	// Extract the actor path
	const FString LandscapeActorPath = InLandscapeActor->GetPathName();

	HAPI_Session const* const HAPISession = FHoudiniEngine::Get().GetSession();

	// Create the attribute
	HAPI_AttributeInfo AttributeInfoLandscapeTarget;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLandscapeTarget);
	AttributeInfoLandscapeTarget.count = InCount;
	AttributeInfoLandscapeTarget.tupleSize = 1;
	AttributeInfoLandscapeTarget.exists = true;
	AttributeInfoLandscapeTarget.owner = InAttribOwner;
	AttributeInfoLandscapeTarget.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoLandscapeTarget.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		HAPISession, InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE,
		&AttributeInfoLandscapeTarget), false);

	// Set the attribute's string data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		LandscapeActorPath, InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE,
		AttributeInfoLandscapeTarget), false);

	return true;
}	

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineOutputAttribute(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const int32 InValue,
	const int32 InCount,
	const HAPI_AttributeOwner InAttribOwner)
{
	if (InNodeId < 0)
		return false;

	HAPI_Session const* const HAPISession = FHoudiniEngine::Get().GetSession();

	// Create the attribute
	HAPI_AttributeInfo AttributeInfoLandscapeSplineOutput;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLandscapeSplineOutput);
	AttributeInfoLandscapeSplineOutput.count = InCount;
	AttributeInfoLandscapeSplineOutput.tupleSize = 1;
	AttributeInfoLandscapeSplineOutput.exists = true;
	AttributeInfoLandscapeSplineOutput.owner = InAttribOwner;
	AttributeInfoLandscapeSplineOutput.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoLandscapeSplineOutput.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		HAPISession, InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE,
		&AttributeInfoLandscapeSplineOutput), false);

	// Set the attribute's string data
	TArray<int32> LandscapeSplineOutput;
	LandscapeSplineOutput.Init(InValue, InCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
		LandscapeSplineOutput, InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE,
		AttributeInfoLandscapeSplineOutput), false);

	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplinePositionAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InPositions)
{
	if (InNodeId < 0 || InPositions.IsEmpty())
		return false;
	
	HAPI_AttributeInfo PositionAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&PositionAttrInfo);
	PositionAttrInfo.tupleSize = 3;
	PositionAttrInfo.count = InPositions.Num() / 3;
	PositionAttrInfo.exists = true;
	PositionAttrInfo.owner = HAPI_ATTROWNER_POINT;
	PositionAttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	PositionAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_ATTRIB_POSITION, &PositionAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InPositions.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_POSITION),
		PositionAttrInfo), false);

	return true;	
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplinePaintLayerNameAttribute(
	const HAPI_NodeId& InNodeId, const TArray<FString>& InPaintLayerNames, const HAPI_AttributeOwner InAttribOwner)
{
	if (InNodeId < 0 || InPaintLayerNames.IsEmpty())
		return false;
	
	HAPI_AttributeInfo LayerNameAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&LayerNameAttrInfo);
	LayerNameAttrInfo.tupleSize = 1;
	LayerNameAttrInfo.count = InPaintLayerNames.Num();
	LayerNameAttrInfo.exists = true;
	LayerNameAttrInfo.owner = InAttribOwner;
	LayerNameAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	LayerNameAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_PAINT_LAYER_NAME, &LayerNameAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InPaintLayerNames, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_PAINT_LAYER_NAME),
		LayerNameAttrInfo), false);

	return true;
}


bool FUnrealLandscapeSplineTranslator::AddLandscapeSplineRaiseTerrainAttribute(
	const HAPI_NodeId& InNodeId, const TArray<int8>& InRaiseTerrain, const HAPI_AttributeOwner InAttribOwner)
{
	if (InNodeId < 0 || InRaiseTerrain.IsEmpty())
		return false;

	HAPI_AttributeInfo RaiseTerrainAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&RaiseTerrainAttrInfo);
	RaiseTerrainAttrInfo.tupleSize = 1;
	RaiseTerrainAttrInfo.count = InRaiseTerrain.Num();
	RaiseTerrainAttrInfo.exists = true;
	RaiseTerrainAttrInfo.owner = InAttribOwner;
	RaiseTerrainAttrInfo.storage = HAPI_STORAGETYPE_INT8;
	RaiseTerrainAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_RAISE_TERRAIN, &RaiseTerrainAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
		InRaiseTerrain, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_RAISE_TERRAIN),
		RaiseTerrainAttrInfo), false);

	return true;
}


bool FUnrealLandscapeSplineTranslator::AddLandscapeSplineLowerTerrainAttribute(
	const HAPI_NodeId& InNodeId, const TArray<int8>& InLowerTerrain, const HAPI_AttributeOwner InAttribOwner)
{
	if (InNodeId < 0 || InLowerTerrain.IsEmpty())
		return false;
	
	HAPI_AttributeInfo LowerTerrainAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&LowerTerrainAttrInfo);
	LowerTerrainAttrInfo.tupleSize = 1;
	LowerTerrainAttrInfo.count = InLowerTerrain.Num();
	LowerTerrainAttrInfo.exists = true;
	LowerTerrainAttrInfo.owner = InAttribOwner;
	LowerTerrainAttrInfo.storage = HAPI_STORAGETYPE_INT8;
	LowerTerrainAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_LOWER_TERRAIN,
		&LowerTerrainAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
		InLowerTerrain, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_LOWER_TERRAIN),
		LowerTerrainAttrInfo), false);

	return true;
}

bool FUnrealLandscapeSplineTranslator::AddLandscapeSplineSegmentMeshesAttributes(
	const HAPI_NodeId& InNodeId, const TArray<FLandscapeSplineSegmentMeshData>& InPerMeshSegmentData)
{
	if (InNodeId < 0)
		return false;

	const int32 NumMeshAttrs = InPerMeshSegmentData.Num();
	if (NumMeshAttrs <= 0)
		return false;

	// The InPerMeshSegmentData[0].MeshRefs array contains the values for unreal_landscape_spline_segment_mesh for all segments
	// InPerMeshSegmentData[1].MeshRefs array contains the values for unreal_landscape_spline_segment_mesh1 for all segments etc
	const int32 NumSegments = InPerMeshSegmentData[0].MeshRefs.Num();

	// Attribute info the mesh references: unreal_landscape_spline_segment_mesh#
	HAPI_AttributeInfo MeshAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MeshAttrInfo);
	MeshAttrInfo.tupleSize = 1;
	MeshAttrInfo.count = NumSegments;
	MeshAttrInfo.exists = true;
	MeshAttrInfo.owner = HAPI_ATTROWNER_PRIM;
	MeshAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MeshAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	// Attribute info the material overrides: unreal_landscape_spline_segment_mesh#_material_override#
	HAPI_AttributeInfo MatOverrideAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MatOverrideAttrInfo);
	MatOverrideAttrInfo.tupleSize = 1;
	MatOverrideAttrInfo.count = NumSegments;
	MatOverrideAttrInfo.exists = true;
	MatOverrideAttrInfo.owner = HAPI_ATTROWNER_PRIM;
	MatOverrideAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MatOverrideAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	// Attribute info the mesh scales: unreal_landscape_spline_segment_mesh#_scale
	HAPI_AttributeInfo MeshScaleAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MeshScaleAttrInfo);
	MeshScaleAttrInfo.tupleSize = 3;
	MeshScaleAttrInfo.count = NumSegments;
	MeshScaleAttrInfo.exists = true;
	MeshScaleAttrInfo.owner = HAPI_ATTROWNER_PRIM;
	MeshScaleAttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	MeshScaleAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	bool bNeedToCommit = false;
	for (int32 MeshIdx = 0; MeshIdx < NumMeshAttrs; ++MeshIdx)
	{
		const FLandscapeSplineSegmentMeshData& MeshSegmentData = InPerMeshSegmentData[MeshIdx];

		// Add the mesh attribute
		const FString MeshAttrName = (MeshIdx == 0
			? FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH))
			: FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH "%d"), MeshIdx));
		HAPI_Result Result;
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::AddAttribute(Session, InNodeId, 0, TCHAR_TO_ANSI(*MeshAttrName), &MeshAttrInfo));
		if (HAPI_RESULT_SUCCESS == Result)
		{
			// Send the values to Houdini
			HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniEngineUtils::HapiSetAttributeStringData(
				MeshSegmentData.MeshRefs, InNodeId, 0, MeshAttrName, MeshAttrInfo));
			if (HAPI_RESULT_SUCCESS == Result)
				bNeedToCommit = true;				
		}

		// Add the mesh scale attribute
		const FString MeshScaleAttrName = FString::Printf(TEXT("%s%s"), *MeshAttrName, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX));
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::AddAttribute(Session, InNodeId, 0, TCHAR_TO_ANSI(*MeshScaleAttrName), &MeshScaleAttrInfo));
		if (HAPI_RESULT_SUCCESS == Result)
		{
			// Send the values to Houdini
			HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniEngineUtils::HapiSetAttributeFloatData(
				MeshSegmentData.MeshScales, InNodeId, 0, MeshScaleAttrName, MeshScaleAttrInfo));
			if (HAPI_RESULT_SUCCESS == Result)
				bNeedToCommit = true;				
		}

		// Material overrides
		const int32 NumMaterialOverrides = MeshSegmentData.MeshMaterialOverrideRefs.Num();
		if (NumMaterialOverrides > 0)
		{
			const FString MaterialOverrideAttrNamePrefix = FString::Printf(TEXT("%s%s"), *MeshAttrName, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX));
			for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
			{
				// Add the material override attribute
				const FString MaterialOverrideAttrName = (MaterialOverrideIdx == 0
					? MaterialOverrideAttrNamePrefix
					: FString::Printf(TEXT("%s%d"), *MaterialOverrideAttrNamePrefix, MaterialOverrideIdx));
				if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(Session, InNodeId, 0, TCHAR_TO_ANSI(*MaterialOverrideAttrName), &MatOverrideAttrInfo))
				{
					// Send the values to Houdini
					HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniEngineUtils::HapiSetAttributeStringData(
						MeshSegmentData.MeshMaterialOverrideRefs[MaterialOverrideIdx], InNodeId, 0, MaterialOverrideAttrName, MatOverrideAttrInfo));
					if (HAPI_RESULT_SUCCESS == Result)
						bNeedToCommit = true;				
				}
			}
		}
	}
	
	return bNeedToCommit;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineConnectionSocketNameAttribute(
	const HAPI_NodeId& InNodeId, const TArray<FString>& InPointConnectionSocketNames)
{
	if (InNodeId < 0 || InPointConnectionSocketNames.IsEmpty())
		return false;
	
	HAPI_AttributeInfo SocketNameAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&SocketNameAttrInfo);
	SocketNameAttrInfo.tupleSize = 1;
	SocketNameAttrInfo.count = InPointConnectionSocketNames.Num();
	SocketNameAttrInfo.exists = true;
	SocketNameAttrInfo.owner = HAPI_ATTROWNER_POINT;
	SocketNameAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	SocketNameAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SOCKET_NAME,
		&SocketNameAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InPointConnectionSocketNames, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SOCKET_NAME),
		SocketNameAttrInfo), false);

	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointRotationAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InControlPointRotations)
{
	if (InNodeId < 0 || InControlPointRotations.IsEmpty())
		return false;
	
	HAPI_AttributeInfo RotationAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&RotationAttrInfo);
	RotationAttrInfo.tupleSize = 4;
	RotationAttrInfo.count = InControlPointRotations.Num() / 4;
	RotationAttrInfo.exists = true;
	RotationAttrInfo.owner = HAPI_ATTROWNER_POINT;
	RotationAttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	RotationAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, &RotationAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InControlPointRotations.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_ROTATION),
		RotationAttrInfo), false);

	return true;	
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointMeshAttribute(
	const HAPI_NodeId& InNodeId, const TArray<FString>& InMeshRefs)
{
	if (InNodeId < 0 || InMeshRefs.IsEmpty())
		return false;
	
	HAPI_AttributeInfo MeshAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MeshAttrInfo);
	MeshAttrInfo.tupleSize = 1;
	MeshAttrInfo.count = InMeshRefs.Num();
	MeshAttrInfo.exists = true;
	MeshAttrInfo.owner = HAPI_ATTROWNER_POINT;
	MeshAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MeshAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH,
		&MeshAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InMeshRefs, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH),
		MeshAttrInfo), false);

	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointMaterialOverrideAttributes(
	const HAPI_NodeId& InNodeId, const TArray<TArray<FString>>& InMaterialOverrideRefs)
{
	if (InNodeId < 0)
		return false;

	const int32 NumMaterialOverrides = InMaterialOverrideRefs.Num();
	if (NumMaterialOverrides <= 0)
		return false;

	// The InMaterialOverrideRefs[0] array contains the values for unreal_landscape_spline_mesh_material_override for all control points
	// InPerMeshSegmentData[1] array contains the values for unreal_landscape_spline_mesh_material_override1 for all control points etc
	const int32 NumControlPoints = InMaterialOverrideRefs[0].Num();

	// Attribute info the mesh references: unreal_landscape_spline_mesh_material_override#
	HAPI_AttributeInfo MatOverrideAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MatOverrideAttrInfo);
	MatOverrideAttrInfo.tupleSize = 1;
	MatOverrideAttrInfo.count = NumControlPoints;
	MatOverrideAttrInfo.exists = true;
	MatOverrideAttrInfo.owner = HAPI_ATTROWNER_PRIM;
	MatOverrideAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MatOverrideAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	bool bNeedToCommit = false;
	const FString AttrNamePrefix(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX));
	for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
	{
		// Add the material override attribute
		const FString MaterialOverrideAttrName = (MaterialOverrideIdx == 0
			? AttrNamePrefix 
			: FString::Printf(TEXT("%s%d"), *AttrNamePrefix, MaterialOverrideIdx));
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(Session, InNodeId, 0, TCHAR_TO_ANSI(*MaterialOverrideAttrName), &MatOverrideAttrInfo))
		{
			// Send the values to Houdini
			HAPI_Result Result;
			HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniEngineUtils::HapiSetAttributeStringData(
				InMaterialOverrideRefs[MaterialOverrideIdx], InNodeId, 0, MaterialOverrideAttrName, MatOverrideAttrInfo));
			if (HAPI_RESULT_SUCCESS == Result)
				bNeedToCommit = true;				
		}
	}
	
	return bNeedToCommit;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointNamesAttribute(
	const HAPI_NodeId& InNodeId, const TArray<FString>& InControlPointNames)
{
	if (InNodeId < 0 || InControlPointNames.IsEmpty())
		return false;
	
	HAPI_AttributeInfo MeshAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MeshAttrInfo);
	MeshAttrInfo.tupleSize = 1;
	MeshAttrInfo.count = InControlPointNames.Num();
	MeshAttrInfo.exists = true;
	MeshAttrInfo.owner = HAPI_ATTROWNER_POINT;
	MeshAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MeshAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_NAME,
		&MeshAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InControlPointNames, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_NAME),
		MeshAttrInfo), false);

	return true;
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineHalfWidthAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InHalfWidths)
{
	if (InNodeId < 0 || InHalfWidths.IsEmpty())
		return false;
	
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = 1;
	AttrInfo.count = InHalfWidths.Num();
	AttrInfo.exists = true;
	AttrInfo.owner = HAPI_ATTROWNER_POINT;
	AttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH, &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InHalfWidths.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH),
		AttrInfo), false);

	return true;	
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineTangentLengthAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InTangentLengths)
{
	if (InNodeId < 0 || InTangentLengths.IsEmpty())
		return false;
	
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = 1;
	AttrInfo.count = InTangentLengths.Num();
	AttrInfo.exists = true;
	AttrInfo.owner = HAPI_ATTROWNER_POINT;
	AttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TANGENT_LENGTH, &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InTangentLengths.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TANGENT_LENGTH),
		AttrInfo), false);

	return true;	
}

bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointMeshScaleAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InMeshScales)
{
	if (InNodeId < 0 || InMeshScales.IsEmpty())
		return false;
	
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = 3;
	AttrInfo.count = InMeshScales.Num() / 3;
	AttrInfo.exists = true;
	AttrInfo.owner = HAPI_ATTROWNER_POINT;
	AttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	const FString AttrName = TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX);

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, TCHAR_TO_ANSI(*AttrName), &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InMeshScales.GetData(), InNodeId, 0, AttrName, AttrInfo), false);

	return true;
}
