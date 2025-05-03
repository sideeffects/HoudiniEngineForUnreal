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

#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniOutput.h"
#include "LandscapeEdit.h"
#include "HoudiniAssetComponent.h"
#include "Landscape.h"
#include "HoudiniAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "Runtime/Launch/Resources/Version.h"

void 
FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData(UHoudiniOutput* InOutput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData);

	TSet<ALandscape*> LandscapesToDelete;
	TArray<FHoudiniOutputObjectIdentifier> OutputObjectsToDelete;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& OutputObjectPair : OutputObjects)
	{
		const FHoudiniOutputObject& PrevObj = OutputObjectPair.Value;
		if (IsValid(PrevObj.OutputObject) && PrevObj.OutputObject->IsA<UHoudiniLandscapeTargetLayerOutput>())
		{
			// Get the layer data stored during cooking
			UHoudiniLandscapeTargetLayerOutput* OldLayer = Cast<UHoudiniLandscapeTargetLayerOutput>(PrevObj.OutputObject);

			OutputObjectsToDelete.Add(OutputObjectPair.Key);

			if (IsValid(OldLayer->Landscape))
			{
				// Delete the edit layers
				if (OldLayer->BakedEditLayer != OldLayer->CookedEditLayer)
				{
					DeleteEditLayer(OldLayer->Landscape, FName(OldLayer->CookedEditLayer));
				}

				// Remove any landscapes that were created.
				if (OldLayer->bCreatedLandscape && OldLayer->Landscape)
					LandscapesToDelete.Add(OldLayer->Landscape);
			}

			PrevObj.OutputObject->ConditionalBeginDestroy();
		}
	}

	// Dont delete everything as we need to keep previous existing output data for the other output objectss!
	// This prevents reusing the data/components/objects from previous cooks
	//InOutput->GetOutputObjects().Empty();	

	// Just delete the output objects we marked as to be deleted
	for (auto OutputObjectIdentifierToDelete : OutputObjectsToDelete)
	{
		OutputObjects.Remove(OutputObjectIdentifierToDelete);
	}

	for (ALandscape* Landscape : LandscapesToDelete)
		DestroyLandscape(Landscape);
}

void FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(ALandscape* Landscape, const FName& LayerName)
{
#if WITH_EDITOR
	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	if (EditLayerIndex == INDEX_NONE)
		return;
	Landscape->DeleteLayer(EditLayerIndex);
#endif
}


void
FHoudiniLandscapeRuntimeUtils::DestroyLandscape(ALandscape* Landscape)
{
	if (!IsValid(Landscape))
		return;

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!IsValid(Info))
		return;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Info->StreamingProxies;
	for (auto ProxyPtr : Proxies)
	{
		ALandscapeStreamingProxy* Proxy = ProxyPtr.Get();
#else
	TArray<ALandscapeStreamingProxy*> Proxies = Info->Proxies;
	for (ALandscapeStreamingProxy* Proxy : Proxies)
	{
#endif	
		if (!IsValid(Proxy))
			continue;

		Info->UnregisterActor(Proxy);
		FHoudiniLandscapeRuntimeUtils::DestroyLandscapeProxy(Proxy);
	}
	Landscape->Destroy();
}

void
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeProxy(ALandscapeProxy* Proxy)
{
	// UE5 does not automatically delete streaming proxies, so clean them up before detroying the parent actor.
	// Note the Proxies array must be copied, not referenced, as it is modified when each Proxy is destroyed.

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->StreamingProxies;
#else
	TArray<TObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->Proxies;
#endif

	for (TWeakObjectPtr<ALandscapeStreamingProxy> Child : Proxies)
	{
		if (Child.Get() != nullptr && IsValid(Child.Get()))
			Child.Get()->Destroy();
	}

	// Once we've removed the streaming proxies delete the landscape actor itself.

	Proxy->Destroy();
}


bool
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesControlPoint(
	ULandscapeSplineControlPoint* InControlPoint,
	const bool bInDestroyOnlyIfUnused,
	const bool bInRemoveDestroyedControlPointFromComponent)
{
	if (!IsValid(InControlPoint))
		return false;

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	HOUDINI_LOG_WARNING(TEXT("Landscape Spline Output is only supported in UE5.1+"));
	return false;
#else
#if WITH_EDITOR
	InControlPoint->DeleteSplinePoints();
#endif

	// Remove the point from connected segments
	for (FLandscapeSplineConnection& Connection : InControlPoint->ConnectedSegments)
	{
		if (!IsValid(Connection.Segment))
			continue;

		if (bInDestroyOnlyIfUnused)
			return false;

#if WITH_EDITOR
		Connection.Segment->DeleteSplinePoints();
#endif

		// Get the control point at the *other* end of the segment and remove it from it
		ULandscapeSplineControlPoint* OtherEnd = Connection.GetFarConnection().ControlPoint;
		if (!IsValid(OtherEnd))
			continue;

		OtherEnd->ConnectedSegments.Remove(FLandscapeSplineConnection(Connection.Segment, 1 - Connection.End));
#if WITH_EDITOR
		OtherEnd->UpdateSplinePoints();
#endif
	}

	InControlPoint->ConnectedSegments.Empty();

	if (bInRemoveDestroyedControlPointFromComponent)
	{
		ULandscapeSplinesComponent* const SplinesComponent = InControlPoint->GetOuterULandscapeSplinesComponent();
		if (IsValid(SplinesComponent))
			SplinesComponent->GetControlPoints().Remove(InControlPoint);
	}

	return true;
#endif
}

bool
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesSegment(
	ULandscapeSplineSegment* InSegment,
	const bool bInRemoveSegmentFromComponent,
	const bool bInDestroyUnusedControlPoints,
	const bool bInRemoveDestroyedControlPointsFromComponent,
	TArray<ULandscapeSplineControlPoint*>* const RemainingControlPoints)
{
	if (!IsValid(InSegment))
		return false;

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	HOUDINI_LOG_WARNING(TEXT("Landscape Spline Output is only supported in UE5.1+"));
	return false;
#else
#if WITH_EDITOR
	InSegment->DeleteSplinePoints();
#endif

	ULandscapeSplineControlPoint* CP0 = InSegment->Connections[0].ControlPoint;
	ULandscapeSplineControlPoint* CP1 = InSegment->Connections[1].ControlPoint;
	const bool bCP0Valid = IsValid(CP0);
	const bool bCP1Valid = IsValid(CP1);
	if (bCP0Valid)
		CP0->ConnectedSegments.Remove(FLandscapeSplineConnection(InSegment, 0));
	if (bCP1Valid)
		CP1->ConnectedSegments.Remove(FLandscapeSplineConnection(InSegment, 1));

#if WITH_EDITOR
	if (bCP0Valid)
		CP0->UpdateSplinePoints();
	if (bCP1Valid)
		CP1->UpdateSplinePoints();
#endif

	ULandscapeSplinesComponent* const SplinesComponent = InSegment->GetOuterULandscapeSplinesComponent();
	const bool bIsSplinesComponentValid = IsValid(SplinesComponent);

	if (bInDestroyUnusedControlPoints)
	{
		if (CP0->ConnectedSegments.Num() == 0)
		{
#if WITH_EDITOR
			CP0->DeleteSplinePoints();
#endif
			CP0->ConnectedSegments.Empty();
			if (bInRemoveDestroyedControlPointsFromComponent && bIsSplinesComponentValid)
				SplinesComponent->GetControlPoints().Remove(CP0);
			CP0 = nullptr;
		}
		if (CP1->ConnectedSegments.Num() == 0)
		{
#if WITH_EDITOR
			CP1->DeleteSplinePoints();
#endif
			CP1->ConnectedSegments.Empty();
			if (bInRemoveDestroyedControlPointsFromComponent && bIsSplinesComponentValid)
				SplinesComponent->GetControlPoints().Remove(CP1);
			CP1 = nullptr;
		}
	}

	if (RemainingControlPoints)
	{
		if (CP0)
			RemainingControlPoints->Add(CP0);
		if (CP1)
			RemainingControlPoints->Add(CP1);
	}

	if (bInRemoveSegmentFromComponent && bIsSplinesComponentValid)
		SplinesComponent->GetSegments().Remove(InSegment);

	return true;
#endif
}

bool
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesSegmentsAndControlPoints(ULandscapeSplinesComponent* const InSplinesComponent)
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	return false;
#else
	if (!IsValid(InSplinesComponent))
		return false;

	TArray<TObjectPtr<ULandscapeSplineSegment>>& Segments = InSplinesComponent->GetSegments();
	TArray<TObjectPtr<ULandscapeSplineControlPoint>>& ControlPoints = InSplinesComponent->GetControlPoints();

	bool bSuccess = true;
	for (ULandscapeSplineSegment* const Segment : Segments)
	{
		if (!IsValid(Segment))
			continue;

		// We don't have to remove the segment from the component here, or destroy the control points, since we'll
		// empty component's Segments array and destroy all control points below
		static constexpr bool bRemoveSegmentFromComponent = false;
		static constexpr bool bDestroyUnusedControlPoints = false;
		static constexpr bool bRemoveDestroyedControlPointsFromComponent = false;
		if (!DestroyLandscapeSplinesSegment(
			Segment,
			bRemoveSegmentFromComponent,
			bDestroyUnusedControlPoints,
			bRemoveDestroyedControlPointsFromComponent))
		{
			bSuccess = false;
		}
	}
	Segments.Empty();

	for (ULandscapeSplineControlPoint* const ControlPoint : ControlPoints)
	{
		if (!IsValid(ControlPoint))
			continue;

		// We don't have to remove the control points from the component here as we'll empty component's ControlPoints
		// array below.
		static constexpr bool bDestroyOnlyIfUnused = false;
		static constexpr bool bRemoveControlPointFromComponent = false;
		if (!DestroyLandscapeSplinesControlPoint(ControlPoint, bDestroyOnlyIfUnused, bRemoveControlPointFromComponent))
			bSuccess = false;
	}
	ControlPoints.Empty();

	return bSuccess;
#endif
}

bool
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesSegmentsAndControlPoints(UHoudiniLandscapeSplinesOutput* const InOutputObject)
{
	
	if (!IsValid(InOutputObject))
		return false;

	bool bSuccess = true;
	for (ULandscapeSplineSegment* const Segment : InOutputObject->GetSegments())
	{
		if (!IsValid(Segment))
			continue;
		static constexpr bool bRemoveSegmentFromComponent = true;
		static constexpr bool bDestroyUnusedControlPoints = false;
		static constexpr bool bRemoveDestroyedControlPointsFromComponent = false;
		if (!DestroyLandscapeSplinesSegment(
			Segment,
			bRemoveSegmentFromComponent,
			bDestroyUnusedControlPoints,
			bRemoveDestroyedControlPointsFromComponent))
		{
			bSuccess = false;
		}
	}
	InOutputObject->GetSegments().Empty();

	for (ULandscapeSplineControlPoint* const ControlPoint : InOutputObject->GetControlPoints())
	{
		if (!IsValid(ControlPoint))
			continue;

		static constexpr bool bDestroyOnlyIfUnused = true;
		static constexpr bool bRemoveControlPointFromComponent = true;
		if (!DestroyLandscapeSplinesControlPoint(ControlPoint, bDestroyOnlyIfUnused, bRemoveControlPointFromComponent))
			bSuccess = false;
	}
	InOutputObject->GetControlPoints().Empty();

	// Return false if we had any failures
	return bSuccess;
}


void
FHoudiniLandscapeRuntimeUtils::DeleteLandscapeSplineCookedData(UHoudiniOutput* const InOutput)
{
	if (!IsValid(InOutput))
		return;

	TArray<FHoudiniOutputObjectIdentifier> OutputObjectsToDelete;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& OutputObjectPair : OutputObjects)
	{
		const FHoudiniOutputObject& PrevObj = OutputObjectPair.Value;
		if (!IsValid(PrevObj.OutputObject) || !PrevObj.OutputObject->IsA<UHoudiniLandscapeSplinesOutput>())
			continue;

		// Get the data stored during cooking
		UHoudiniLandscapeSplinesOutput* const OldOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(PrevObj.OutputObject);
		OldOutputObject->Clear();

		OutputObjectsToDelete.Add(OutputObjectPair.Key);

		OldOutputObject->ConditionalBeginDestroy();
	}

	// Delete the output objects we marked as to be deleted
	for (const auto& OutputObjectIdentifierToDelete : OutputObjectsToDelete)
	{
		OutputObjects.Remove(OutputObjectIdentifierToDelete);
	}
}


int32
FHoudiniLandscapeRuntimeUtils::GetOrGenerateValidControlPointId(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	TObjectPtr<ULandscapeSplineControlPoint>& InControlPoint,
#else
	ULandscapeSplineControlPoint const* const InControlPoint,
#endif
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
	int32& InNextControlPointId)
{
	// Look for the id of ControlPoint in the map
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const TSoftObjectPtr<ULandscapeSplineControlPoint> ControlPointPtr(InControlPoint.Get());
#else
	const TSoftObjectPtr<ULandscapeSplineControlPoint> ControlPointPtr(InControlPoint);
#endif
	int32 ControlPointId = INDEX_NONE;
	if (int32 const* const ControlPointIdPtr = InControlPointIdMap.Find(ControlPointPtr))
		ControlPointId = *ControlPointIdPtr;

	if (ControlPointId < 0)
	{
		// Control point does not have a valid id, generate one
		ControlPointId = InNextControlPointId;
		InNextControlPointId++;

		// Add / update the ControlPoint's id to / in the map
		InControlPointIdMap.Add(ControlPointPtr, ControlPointId);
	}

	return ControlPointId;
}
