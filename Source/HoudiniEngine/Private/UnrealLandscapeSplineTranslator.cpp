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
#include "HoudiniLandscapeRuntimeUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"

#include "Landscape.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "Materials/MaterialInterface.h"


class FLandscapeSplineControlPointAttributes
{
public:
	/** Empties all arrays and reserve enough space for InExpectedPointCount entries. */ 
	void Init(int32 InExpectedPointCount);

	/** Add an entry to each array with the property values from InControlPoint. */
	bool AddControlPointData(
		ULandscapeSplineControlPoint const* InControlPoint,
		int32 InControlPointIndex,
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId);

	/** Add an empty / default initialized entry to each array. */
	void AddEmpty();

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

	/** The ids of the control points. */
	TArray<int32> ControlPointIds;

	/** The control point half-width. */
	TArray<float> ControlPointHalfWidths;

	/** The control point side-falloff. */
	TArray<float> ControlPointSideFalloffs;

	/** The control point end-falloff. */
	TArray<float> ControlPointEndFalloffs;

private:
	/** The expected number of points to store attributes for. Set in Init() when reserving space in the arrays. */ 
	int32 ExpectedPointCount = 0;
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
	 * If a point corresponds with a control point on the spline, this contains the control point's tangent length
	 * for the segment connection.
	 */
	TArray<float> PointConnectionTangentLengths;

	/** Control point specific attributes. */
	FLandscapeSplineControlPointAttributes ControlPointAttributes;
};


struct FLandscapeSplinesControlPointData
{
	/**
	 * The control point positions of the splines. These are the original positions unaffected by connection mesh
	 * sockets.
	 */
	TArray<float> ControlPointPositions;

	/** Control point attributes. */ 
	FLandscapeSplineControlPointAttributes Attributes;
};


/**
 * Helper struct for storing unresampled points (center, left and right) and the point's normalized [0, 1] position
 * along the spline.
 */
class FUnResampledPoint
{
public:
	FUnResampledPoint() = delete;
	
	FUnResampledPoint(const EHoudiniLandscapeSplineCurve& InSplineSelection);
	FUnResampledPoint(const EHoudiniLandscapeSplineCurve& InSplineSelection, const FLandscapeSplineInterpPoint& InPoint);
	
	FVector GetSelectedPosition() const;
	FQuat CalculateRotationTo(const FUnResampledPoint& InNextPoint);
	
	FVector Center;
	FVector Left;
	FVector Right;
	FVector FalloffLeft;
	FVector FalloffRight;
	FQuat Rotation;
	float Alpha;
	EHoudiniLandscapeSplineCurve SplineSelection;
};


/**
 * Helper struct to record segment data, such as the segment length, unresampled points (spline points generated in UE),
 * and global segment index (index in the output arrays of data sent to Houdini).
 */
struct FOrderedSegmentData
{
	TObjectPtr<ULandscapeSplineSegment> Segment;
	float SegmentLength = 0.0f;
	TArray<FUnResampledPoint> UnResampledPoints;
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
FLandscapeSplineControlPointAttributes::Init(const int32 InExpectedPointCount)
{
	ExpectedPointCount = InExpectedPointCount;
	ControlPointRotations.Empty(ExpectedPointCount * 4);
	ControlPointPaintLayerNames.Empty(ExpectedPointCount);
	ControlPointRaiseTerrains.Empty(ExpectedPointCount);
	ControlPointLowerTerrains.Empty(ExpectedPointCount);
	ControlPointMeshRefs.Empty(ExpectedPointCount);
	PerMaterialOverrideControlPointRefs.Empty();
	ControlPointMeshScales.Empty(ExpectedPointCount * 3);
	ControlPointIds.Empty(ExpectedPointCount);
	ControlPointHalfWidths.Empty(ExpectedPointCount);
	ControlPointSideFalloffs.Empty(ExpectedPointCount);
	ControlPointEndFalloffs.Empty(ExpectedPointCount);
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
FLandscapeSplineControlPointAttributes::AddControlPointData(
	ULandscapeSplineControlPoint const* const InControlPoint,
	const int32 InControlPointIndex,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId)
{
	if (!IsValid(InControlPoint))
		return false;

	ConvertAndSetRotation(InControlPoint->Rotation, -1, ControlPointRotations); 

	const int32 ControlPointId = FHoudiniLandscapeRuntimeUtils::GetOrGenerateValidControlPointId(
		InControlPoint, InControlPointIdMap, InNextControlPointId);
	ControlPointIds.Emplace(ControlPointId);

	ControlPointHalfWidths.Add(InControlPoint->Width / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	ControlPointSideFalloffs.Add(InControlPoint->SideFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	ControlPointEndFalloffs.Add(InControlPoint->EndFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	ControlPointPaintLayerNames.Emplace(InControlPoint->LayerName.ToString());
	ControlPointRaiseTerrains.Add(InControlPoint->bRaiseTerrain);
	ControlPointLowerTerrains.Add(InControlPoint->bLowerTerrain);

	// Set the static mesh reference 
	ControlPointMeshRefs.Emplace(IsValid(InControlPoint->Mesh) ? InControlPoint->Mesh->GetPathName() : FString());

	const int32 NumMaterialOverrides = InControlPoint->MaterialOverrides.Num();
	if (PerMaterialOverrideControlPointRefs.Num() < NumMaterialOverrides)
		PerMaterialOverrideControlPointRefs.SetNum(NumMaterialOverrides);
	for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < NumMaterialOverrides; ++MaterialOverrideIdx)
	{
		UMaterialInterface const* const Material = InControlPoint->MaterialOverrides[MaterialOverrideIdx];
		// Initialize the per control point array for this override index if necessary
		TArray<FString>& PerCPMaterialOverrideRefs = PerMaterialOverrideControlPointRefs[MaterialOverrideIdx];
		if (PerCPMaterialOverrideRefs.IsEmpty())
			PerCPMaterialOverrideRefs.SetNum(ExpectedPointCount);

		// Set the material ref or empty string if the material is invalid
		PerCPMaterialOverrideRefs[InControlPointIndex] = IsValid(Material) ? Material->GetPathName() : FString(); 
	}

	ControlPointMeshScales.Add(InControlPoint->MeshScale.X);
	ControlPointMeshScales.Add(InControlPoint->MeshScale.Z);
	ControlPointMeshScales.Add(InControlPoint->MeshScale.Y);

	return true;
}


void
FLandscapeSplineControlPointAttributes::AddEmpty()
{
	ControlPointRotations.Add(FQuat::Identity.X);
	ControlPointRotations.Add(FQuat::Identity.Z);
	ControlPointRotations.Add(FQuat::Identity.Y);
	ControlPointRotations.Add(-FQuat::Identity.W);

	ControlPointIds.Add(INDEX_NONE);

	ControlPointHalfWidths.AddDefaulted();
	ControlPointSideFalloffs.AddDefaulted();
	ControlPointEndFalloffs.AddDefaulted();

	ControlPointPaintLayerNames.AddDefaulted();
	ControlPointRaiseTerrains.Add(false);
	ControlPointLowerTerrains.Add(false);

	ControlPointMeshRefs.AddDefaulted();

	ControlPointMeshScales.Add(1.0f);
	ControlPointMeshScales.Add(1.0f);
	ControlPointMeshScales.Add(1.0f);
}


FUnResampledPoint::FUnResampledPoint(const EHoudiniLandscapeSplineCurve& InSplineSelection)
	: Alpha(0.0f)
	, SplineSelection(InSplineSelection)
{
}

FUnResampledPoint::FUnResampledPoint(const EHoudiniLandscapeSplineCurve& InSplineSelection, const FLandscapeSplineInterpPoint& InPoint)
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
FUnResampledPoint::GetSelectedPosition() const
{
	switch (SplineSelection)
	{
	case EHoudiniLandscapeSplineCurve::Center:
		return Center;
	case EHoudiniLandscapeSplineCurve::Left:
		return Left;
	case EHoudiniLandscapeSplineCurve::Right:
		return Right;
	default:
		HOUDINI_LOG_WARNING(TEXT("Invalid value for SplineSelection: %d, returning Center point."), SplineSelection);
		break;
	}
	
	return Center;
}

FQuat
FUnResampledPoint::CalculateRotationTo(const FUnResampledPoint& InNextPoint)
{
	const FVector ForwardVector = (InNextPoint.Center - Center).GetSafeNormal();
	const FVector RightVector = (Right - Center).GetSafeNormal();
	Rotation = FRotationMatrix::MakeFromXY(ForwardVector, RightVector).ToQuat();
	return Rotation;
}

bool
FUnrealLandscapeSplineTranslator::CreateInputNodeForLandscapeSplinesComponent(
	ULandscapeSplinesComponent* const InSplinesComponent, 
	const bool bForceReferenceInputNodeCreation,
	HAPI_NodeId& OutCreatedInputNodeId,
	FUnrealObjectInputHandle& OutInputNodeHandle,
	const FString& InNodeName,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	const float InSplineResolution,
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
	const bool bUseRefCountedInputSystem = FUnrealObjectInputRuntimeUtils::IsRefCountedInputSystemEnabled();
	if (bUseRefCountedInputSystem)
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
				if (!CreateInputNodeForLandscapeSplinesComponent(
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
		if (!CreateInputNodeForLandscapeSplines(
				InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
				SplinesNodeId,	EHoudiniLandscapeSplineCurve::Center, InSplineResolution))
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
		if (!CreateInputNodeForLandscapeSplines(
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
			SplinesNodeId, EHoudiniLandscapeSplineCurve::Left, InSplineResolution))
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
			InSplinesComponent, ObjectNodeId, FinalInputNodeName, InControlPointIdMap, InNextControlPointId,
			SplinesNodeId, EHoudiniLandscapeSplineCurve::Right, InSplineResolution))
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
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, OutCreatedInputNodeId, Handle, InputObjectNodeId, ReferencedNodes, bInInputNodesCanBeDeleted))
			OutInputNodeHandle = Handle;
	}

	return bSuccess;
}


bool
FUnrealLandscapeSplineTranslator::CreateInputNodeForLandscapeSplines(
	ULandscapeSplinesComponent* const InSplinesComponent,
	const HAPI_NodeId& InObjectNodeId,
	const FString& InNodeName,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	HAPI_NodeId& OutNodeId,
	const EHoudiniLandscapeSplineCurve InExportCurve,
	const float InSplineResolution)
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
	if (!ExtractLandscapeSplineData(InSplinesComponent, InControlPointIdMap, InNextControlPointId, SplinesData, InExportCurve, InSplineResolution))
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

	if (AddLandscapeSplineControlPointAttributes(OutNodeId, SplinesData.ControlPointAttributes))
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

	// Segment connection attributes (point attributes)
	if (AddLandscapeSplineTangentLengthAttribute(OutNodeId, SplinesData.PointConnectionTangentLengths))
		bNeedToCommit = true;

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
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	HAPI_NodeId& OutNodeId)
{
	if (!IsValid(InSplinesComponent))
		return false;

	// Set the final node name with _control_points suffix
	const FString FinalInputNodeName = InNodeName + TEXT("_control_points");

	FLandscapeSplinesControlPointData ControlPointsData;
	if (!ExtractLandscapeSplineControlPointsData(InSplinesComponent, InControlPointIdMap, InNextControlPointId, ControlPointsData))
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

	if (AddLandscapeSplineControlPointAttributes(OutNodeId, ControlPointsData.Attributes))
		bNeedToCommit = true;

	// Add the unreal_landscape_spline_output attribute to indicate that this a landscape spline and not a normal curve
	// TODO: Should there be a special type / value for the control points
	if (AddLandscapeSplineOutputAttribute(OutNodeId, 0, 1, PartInfo.pointCount, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT))
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
	const EHoudiniLandscapeSplineCurve InExportCurve,
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
				FUnResampledPoint& Point0 = SegmentData.UnResampledPoints[VertIdx - 1];
				FUnResampledPoint& Point1 = SegmentData.UnResampledPoints.Emplace_GetRef(
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
					const FUnResampledPoint& Point0 = SegmentData.UnResampledPoints[VertIdx - 1];
					const FUnResampledPoint& Point1 = SegmentData.UnResampledPoints[VertIdx];
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

bool FUnrealLandscapeSplineTranslator::ExtractLandscapeSplineData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
	FLandscapeSplinesData& OutSplinesData,
	const EHoudiniLandscapeSplineCurve InExportCurve,
	const float InSplineResolution)
{
	if (!IsValid(InSplinesComponent))
		return false;

	if (!InSplinesComponent->HasAnyControlPointsOrSegments())
		return false;

	if (InExportCurve == EHoudiniLandscapeSplineCurve::Invalid)
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid value for InExportCurve: %d, aborting landscape spline data extraction."), InExportCurve);
		return false;
	}

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
			
			FUnResampledPoint UnResampledPoint0 = SegmentData.UnResampledPoints[0];
			FUnResampledPoint UnResampledPoint1 = SegmentData.UnResampledPoints[1];
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
					ULandscapeSplineControlPoint const* const CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
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
					ULandscapeSplineControlPoint const* const CPoint = SegmentData.Segment->Connections[ConnectionIdx].ControlPoint;
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
					OutSplinesData.ControlPointAttributes.ControlPointHalfWidths.Last() = CalculatedHalfWidth / HAPI_UNREAL_SCALE_FACTOR_POSITION;
					OutSplinesData.ControlPointAttributes.ControlPointSideFalloffs.Last() = CalculatedSideFalloff / HAPI_UNREAL_SCALE_FACTOR_POSITION;
					// We don't have calculated end-falloff values for non-control points, set to 0
					OutSplinesData.ControlPointAttributes.ControlPointEndFalloffs.Last() = 0.0f;
					// Set the calculated / resampled rotation
					ConvertAndSetRotation(
						ResampledRotation, OutSplinesData.ControlPointAttributes.ControlPointRotations.Num() - 4, OutSplinesData.ControlPointAttributes.ControlPointRotations);
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
				FLandscapeSplineSegmentMeshData& SegmentMeshData = OutSplinesData.PerMeshSegmentData[MeshIdx];
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
FUnrealLandscapeSplineTranslator::ExtractLandscapeSplineControlPointsData(
	ULandscapeSplinesComponent* const InSplinesComponent,
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId,
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
	OutSplinesControlPointData.Attributes.Init(NumControlPoints);
	
	for (int32 ControlPointIdx = 0; ControlPointIdx < NumControlPoints; ++ControlPointIdx)
	{
		ULandscapeSplineControlPoint const* const CPoint = ControlPoints[ControlPointIdx];
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

	const FString AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_PAINT_LAYER_NAME
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &LayerNameAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InPaintLayerNames, InNodeId, 0, AttributeName, LayerNameAttrInfo), false);

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

	const FString AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_RAISE_TERRAIN
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &RaiseTerrainAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
		InRaiseTerrain, InNodeId, 0, AttributeName, RaiseTerrainAttrInfo), false);

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

	const FString AttributeName = InAttribOwner == HAPI_ATTROWNER_POINT
		? HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_LOWER_TERRAIN
		: HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &LowerTerrainAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
		InLowerTerrain, InNodeId, 0, AttributeName, LowerTerrainAttrInfo), false);

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
			? FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH))
			: FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH "%d"), MeshIdx));
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
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH,
		&MeshAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		InMeshRefs, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH),
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
	MatOverrideAttrInfo.owner = HAPI_ATTROWNER_POINT;
	MatOverrideAttrInfo.storage = HAPI_STORAGETYPE_STRING;
	MatOverrideAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	bool bNeedToCommit = false;
	const FString AttrNamePrefix(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX));
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
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointIdsAttribute(
	const HAPI_NodeId& InNodeId, const TArray<int32>& InControlPointIds)
{
	if (InNodeId < 0 || InControlPointIds.IsEmpty())
		return false;
	
	HAPI_AttributeInfo MeshAttrInfo;
	FHoudiniApi::AttributeInfo_Init(&MeshAttrInfo);
	MeshAttrInfo.tupleSize = 1;
	MeshAttrInfo.count = InControlPointIds.Num();
	MeshAttrInfo.exists = true;
	MeshAttrInfo.owner = HAPI_ATTROWNER_POINT;
	MeshAttrInfo.storage = HAPI_STORAGETYPE_INT;
	MeshAttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_ID,
		&MeshAttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
		InControlPointIds, InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_ID),
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
FUnrealLandscapeSplineTranslator::AddLandscapeSplineSideFalloffAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InSideFalloffs)
{
	if (InNodeId < 0 || InSideFalloffs.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = 1;
	AttrInfo.count = InSideFalloffs.Num();
	AttrInfo.exists = true;
	AttrInfo.owner = HAPI_ATTROWNER_POINT;
	AttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SIDE_FALLOFF, &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InSideFalloffs.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SIDE_FALLOFF),
		AttrInfo), false);

	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineEndFalloffAttribute(
	const HAPI_NodeId& InNodeId, const TArray<float>& InEndFalloffs)
{
	if (InNodeId < 0 || InEndFalloffs.IsEmpty())
		return false;

	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = 1;
	AttrInfo.count = InEndFalloffs.Num();
	AttrInfo.exists = true;
	AttrInfo.owner = HAPI_ATTROWNER_POINT;
	AttrInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_END_FALLOFF, &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InEndFalloffs.GetData(), InNodeId, 0, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_END_FALLOFF),
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

	const FString AttrName = TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX);

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		Session, InNodeId, 0, TCHAR_TO_ANSI(*AttrName), &AttrInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		InMeshScales.GetData(), InNodeId, 0, AttrName, AttrInfo), false);

	return true;
}


bool
FUnrealLandscapeSplineTranslator::AddLandscapeSplineControlPointAttributes(
	const HAPI_NodeId& InNodeId, const FLandscapeSplineControlPointAttributes& InControlPointAttributes)
{
	if (InNodeId < 0)
		return false;
	
	bool bNeedToCommit = false;

	if (AddLandscapeSplineControlPointRotationAttribute(InNodeId, InControlPointAttributes.ControlPointRotations))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointIdsAttribute(InNodeId, InControlPointAttributes.ControlPointIds))
		bNeedToCommit = true;

	if (AddLandscapeSplineHalfWidthAttribute(InNodeId, InControlPointAttributes.ControlPointHalfWidths))
		bNeedToCommit = true;

	if (AddLandscapeSplineSideFalloffAttribute(InNodeId, InControlPointAttributes.ControlPointSideFalloffs))
		bNeedToCommit = true;

	if (AddLandscapeSplineEndFalloffAttribute(InNodeId, InControlPointAttributes.ControlPointEndFalloffs))
		bNeedToCommit = true;

	if (AddLandscapeSplinePaintLayerNameAttribute(InNodeId, InControlPointAttributes.ControlPointPaintLayerNames, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineRaiseTerrainAttribute(InNodeId, InControlPointAttributes.ControlPointRaiseTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineLowerTerrainAttribute(InNodeId, InControlPointAttributes.ControlPointLowerTerrains, HAPI_ATTROWNER_POINT))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointMeshAttribute(InNodeId, InControlPointAttributes.ControlPointMeshRefs))
		bNeedToCommit = true;
	
	if (AddLandscapeSplineControlPointMaterialOverrideAttributes(InNodeId, InControlPointAttributes.PerMaterialOverrideControlPointRefs))
		bNeedToCommit = true;

	if (AddLandscapeSplineControlPointMeshScaleAttribute(InNodeId, InControlPointAttributes.ControlPointMeshScales))
		bNeedToCommit = true;

	return bNeedToCommit;
}
