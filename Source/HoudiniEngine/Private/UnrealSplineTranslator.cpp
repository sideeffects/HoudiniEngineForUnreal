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

#include "UnrealSplineTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniSplineTranslator.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealObjectInputRuntimeUtils.h"

#include "Components/SplineComponent.h"


bool
FUnrealSplineTranslator::CreateInputNodeForSplineComponent(
	USplineComponent* SplineComponent,
	HAPI_NodeId& CreatedInputNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const float& SplineResolution,
	const FString& InputNodeName,
	const bool& bInUseLegacyInputCurves,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(SplineComponent))
		return false;

	// Input node name, defaults to InputNodeName, but can be changed by the new input system
	FString FinalInputNodeName = InputNodeName;

	// Find the node in new input system
	// Identifier will be the identifier for the entry created in this call of the function.
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	{
		// Creates this input's identifier and input options
		FUnrealObjectInputOptions Options;
		Options.UnrealSplineResolution = SplineResolution;
		Options.bUseLegacyInputCurves = bInUseLegacyInputCurves;
		Identifier = FUnrealObjectInputIdentifier(SplineComponent, Options, true);

		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId) && FUnrealObjectInputUtils::AreReferencedHAPINodesValid(Handle))
			{
				if (!bInputNodesCanBeDeleted)
				{
					// Make sure to prevent deletion of the input node if needed
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);
				}

				OutHandle = Handle;
				CreatedInputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// Set CreatedInputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that CreatedInputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedInputNodeId))
				CreatedInputNodeId = -1;
		}
		else
		{
			CreatedInputNodeId = -1;
		}

		// We now need to create the nodes (since we couldn't find existing ones in the manager)
		// To do that, we can simply continue this function
		
		// Set CreatedInputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that CreatedInputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedInputNodeId))
				CreatedInputNodeId = -1;
		}
		else
		{
			CreatedInputNodeId = -1;
		}
	}
	
	int32 NumberOfControlPoints = SplineComponent->GetNumberOfSplinePoints();
	float SplineLength = SplineComponent->GetSplineLength();

	// Calculate the number of refined point we want
	int32 NumberOfRefinedSplinePoints = SplineResolution > 0.0f ? ceil(SplineLength / SplineResolution) + 1 : NumberOfControlPoints;

	TArray<FVector> RefinedSplinePositions;
	TArray<FQuat> RefinedSplineRotations;
	TArray<FVector> RefinedSplineScales;
	if (NumberOfRefinedSplinePoints <= NumberOfControlPoints) 
	{
		// There's not enough refined points, so we'll use the control points instead
		RefinedSplinePositions.SetNumZeroed(NumberOfControlPoints);
		RefinedSplineRotations.SetNumZeroed(NumberOfControlPoints);
		RefinedSplineScales.SetNumZeroed(NumberOfControlPoints);

		for (int32 n = 0; n < NumberOfControlPoints; ++n) 
		{
			RefinedSplinePositions[n] = SplineComponent->GetLocationAtSplinePoint(n, ESplineCoordinateSpace::Local);
			RefinedSplineRotations[n] = SplineComponent->GetQuaternionAtSplinePoint(n, ESplineCoordinateSpace::World);
			RefinedSplineScales[n] = SplineComponent->GetScaleAtSplinePoint(n);
		}
	}
	else 
	{
		// Calculate the refined spline component
		RefinedSplinePositions.SetNumZeroed(NumberOfRefinedSplinePoints);
		RefinedSplineRotations.SetNumZeroed(NumberOfRefinedSplinePoints);
		RefinedSplineScales.SetNumZeroed(NumberOfRefinedSplinePoints);

		float CurrentDistance = 0.0f;
		for (int32 n = 0; n < NumberOfRefinedSplinePoints; ++n) 
		{
			RefinedSplinePositions[n] = SplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::Local);
			RefinedSplineRotations[n] = SplineComponent->GetQuaternionAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
			RefinedSplineScales[n] = SplineComponent->GetScaleAtDistanceAlongSpline(CurrentDistance);

			CurrentDistance += SplineResolution;
		}
	}

	// Currently, we must create legacy curves when using the new input system
	// as the new HAPI_CreateInputCurveNode function does not support choosing a parent node!
	bool bUseLegacy = bInUseLegacyInputCurves;
	bUseLegacy = true;

	if (!FHoudiniSplineTranslator::HapiCreateCurveInputNodeForData(
		CreatedInputNodeId,
		ParentNodeId,
		FinalInputNodeName,
		&RefinedSplinePositions, 
		&RefinedSplineRotations, 
		&RefinedSplineScales,
		EHoudiniCurveType::Polygon, 
		EHoudiniCurveMethod::Breakpoints, 
		SplineComponent->IsClosedLoop(), 
		false,
		false, 
		FTransform::Identity,
		bUseLegacy))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to create the input curve data!"));
		return false;
	}

	// Add spline component tags if it has any
	bool NeedToCommit = FHoudiniEngineUtils::CreateGroupsFromTags(CreatedInputNodeId, 0, SplineComponent->ComponentTags);

	// Add the parent actor's tag if it has any
	AActor* ParentActor = SplineComponent->GetOwner();
	if (IsValid(ParentActor)) 
	{
		if (FHoudiniEngineUtils::CreateGroupsFromTags(CreatedInputNodeId, 0, ParentActor->Tags))
			NeedToCommit = true;

		HAPI_PartInfo PartInfo;
		FHoudiniApi::PartInfo_Init(&PartInfo);
		FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), CreatedInputNodeId, 0, &PartInfo);

		// Add the unreal_actor_path attribute
		if(FHoudiniEngineUtils::AddActorPathAttribute(CreatedInputNodeId, 0, ParentActor, PartInfo.faceCount))
			NeedToCommit = true;

		// Add the unreal_level_path attribute
		if(FHoudiniEngineUtils::AddLevelPathAttribute(CreatedInputNodeId, 0, ParentActor->GetLevel(), PartInfo.faceCount))
			NeedToCommit = true;
	}
	
	if (NeedToCommit) 
	{
		// We successfully added tags to the geo, so we need to commit the changes
		if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiCommitGeo(CreatedInputNodeId))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups for the spline input's tags!"));

		// And cook it with refinement enabled
		// This is mandatory as it fixes issues down the line on the next update!
		// (session was lost upon trying to access the data afterwards)
		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		CookOptions.maxVerticesPerPrimitive = -1;
		CookOptions.refineCurveToLinear = true;
		if (!FHoudiniEngineUtils::HapiCookNode(CreatedInputNodeId, &CookOptions, false))
			return false;
	}

	// Get our parent OBJ NodeID
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedInputNodeId);
	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, CreatedInputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return true;
}
