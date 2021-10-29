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

#include "Components/SplineComponent.h"
#include "HoudiniGeoPartObject.h"

#include "HoudiniSplineTranslator.h"

bool
FUnrealSplineTranslator::CreateInputNodeForSplineComponent(USplineComponent* SplineComponent, const float& SplineResolution, HAPI_NodeId& CreatedInputNodeId, const FString& NodeName) 
{
	if (!IsValid(SplineComponent))
		return false;
	
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


	if (!FHoudiniSplineTranslator::HapiCreateCurveInputNodeForData(CreatedInputNodeId, NodeName,
		&RefinedSplinePositions, &RefinedSplineRotations, &RefinedSplineScales,
		EHoudiniCurveType::Polygon, EHoudiniCurveMethod::Breakpoints, false, SplineComponent->IsClosedLoop()))
		return false;

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
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), CreatedInputNodeId))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups for the spline input's tags!"));
	}


	return true;
}
