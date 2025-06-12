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

#include "HoudiniOutput.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniCookable.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniSplineComponent.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Misc/StringFormatArg.h"
#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "Animation/Skeleton.h"
#include "Templates/Tuple.h"
#include "HoudiniFoliageUtils.h"
#include "Engine/DataTable.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#if defined(HOUIDNI_USE_PCG)
#include "HoudiniPCGDataObject.h"
#endif


FHoudiniMaterialIdentifier::FHoudiniMaterialIdentifier(
		const FString& InMaterialObjectPath,
		const bool bInMakeMaterialInstance, 
		const FString& InMaterialInstanceParametersSlug)
	: MaterialObjectPath(InMaterialObjectPath)
	, bIsHoudiniMaterial(false)
	, bMakeMaterialInstance(bInMakeMaterialInstance)
	, MaterialInstanceParametersSlug(InMaterialInstanceParametersSlug) 
{
}

FHoudiniMaterialIdentifier::FHoudiniMaterialIdentifier(const FString& InMaterialPath, const bool bInIsHoudiniMaterial)
	: MaterialObjectPath(InMaterialPath)
	, bIsHoudiniMaterial(bInIsHoudiniMaterial)
	, bMakeMaterialInstance(false)
	, MaterialInstanceParametersSlug()
{
}

uint32
FHoudiniMaterialIdentifier::GetTypeHash() const
{
	// bMakeMaterialInstance is only relevant if bIsHoudiniMaterial is false
	const bool bNotHoudiniMaterialAndMakeInstance = !bIsHoudiniMaterial && bMakeMaterialInstance; 
	const TTuple<FString, bool, bool, FString> T {
		MaterialObjectPath, bIsHoudiniMaterial, bNotHoudiniMaterialAndMakeInstance, bNotHoudiniMaterialAndMakeInstance ? MaterialInstanceParametersSlug : FString() };
	return ::GetTypeHash(T);
}

bool
FHoudiniMaterialIdentifier::operator==(const FHoudiniMaterialIdentifier& InRhs) const
{
	if (MaterialObjectPath != InRhs.MaterialObjectPath || bIsHoudiniMaterial != InRhs.bIsHoudiniMaterial)
		return false;
	if (bIsHoudiniMaterial)
		return true;
	if (bMakeMaterialInstance != InRhs.bMakeMaterialInstance)
		return false;
	return !bMakeMaterialInstance || MaterialInstanceParametersSlug == InRhs.MaterialInstanceParametersSlug;
}


UHoudiniLandscapePtr::UHoudiniLandscapePtr(class FObjectInitializer const& Initializer) 
{
	// bIsWorldCompositionLandscape = false;
	// BakeType = EHoudiniLandscapeOutputBakeType::Detachment;
};


bool
UHoudiniLandscapeSplinesOutput::GetLayerSegments(const FName InEditLayer, TArray<ULandscapeSplineSegment*>& OutSegments) const
{
	const TObjectPtr<UHoudiniLandscapeSplineTargetLayerOutput> * LayerOutputPtr = LayerOutputs.Find(InEditLayer);
	if (!LayerOutputPtr)
		return false;

	UHoudiniLandscapeSplineTargetLayerOutput* LayerOutput = LayerOutputPtr->Get();
	if (!IsValid(LayerOutput))
		return false;

	OutSegments = LayerOutput->Segments;
	return true;
}


void
UHoudiniLandscapeSplinesOutput::Clear(bool bInClearTempLayers)
{
	// Delete the splines (segments and control points)
	FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesSegmentsAndControlPoints(this);

	// Delete the edit layers
	for (const auto& LayerOutputPair : LayerOutputs)
	{
		UHoudiniLandscapeSplineTargetLayerOutput const* const LayerOutput = LayerOutputPair.Value;
		if (!IsValid(LayerOutput) || !IsValid(LayerOutput->Landscape))
			continue;

		if (bInClearTempLayers && LayerOutput->BakedEditLayer != LayerOutput->CookedEditLayer)
			FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(LayerOutput->Landscape, FName(LayerOutput->CookedEditLayer));
	}

	if (IsValid(LandscapeSplineActor))
	{
		ULandscapeInfo* const LSInfo = LandscapeSplineActor->GetLandscapeInfo();
		if (IsValid(LSInfo))
		{
#if WITH_EDITOR
			LSInfo->UnregisterSplineActor(LandscapeSplineActor);
#endif
			LandscapeSplineActor->Destroy();
		}
	}

	Landscape = nullptr;
	LandscapeProxy = nullptr;
	LandscapeSplineActor = nullptr;
	LandscapeSplinesComponent = nullptr;
	LayerOutputs.Empty();
	Segments.Empty();
	ControlPoints.Empty();
}


uint32
GetTypeHash(const FHoudiniOutputObjectIdentifier& HoudiniOutputObjectIdentifier)
{
	return HoudiniOutputObjectIdentifier.GetTypeHash();
}

// ----------------------------------------------------
// FHoudiniOutputObjectIdentifier
// ----------------------------------------------------

FHoudiniOutputObjectIdentifier::FHoudiniOutputObjectIdentifier()
{
	ObjectId = -1;
	GeoId = -1;
	PartId = -1;
	SplitIdentifier = FString();
	PartName = FString();
}

FHoudiniOutputObjectIdentifier::FHoudiniOutputObjectIdentifier(
	int32 InObjectId, int32 InGeoId, int32 InPartId, const FString& InSplitIdentifier)
{
	ObjectId = InObjectId;
	GeoId = InGeoId;
	PartId = InPartId;
	SplitIdentifier = InSplitIdentifier;
}

uint32
FHoudiniOutputObjectIdentifier::GetTypeHash() const
{
	int32 HashBuffer[3] = { ObjectId, GeoId, PartId };
	int32 Hash = FCrc::MemCrc32((void *)&HashBuffer[0], sizeof(HashBuffer));
	return FCrc::StrCrc32(*SplitIdentifier, Hash);
}

bool
FHoudiniOutputObjectIdentifier::operator==(const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier) const
{
	// Object/Geo/Part IDs must match
	bool bMatchingIds = true;
	if (ObjectId != InOutputObjectIdentifier.ObjectId
		|| GeoId != InOutputObjectIdentifier.GeoId
		|| PartId != InOutputObjectIdentifier.PartId)
		bMatchingIds = false;

	if ((bLoaded && !InOutputObjectIdentifier.bLoaded)
		|| (!bLoaded && InOutputObjectIdentifier.bLoaded))
	{
		// If one of the two identifier is loaded, 
		// we can simply compare the part names
		if (PartName.Equals(InOutputObjectIdentifier.PartName)
			&& SplitIdentifier.Equals(InOutputObjectIdentifier.SplitIdentifier))
			return true;
	}

	if (!bMatchingIds)
	{
		return false;
	}

	// If split ID and name match, we're equal...
	if (SplitIdentifier.Equals(InOutputObjectIdentifier.SplitIdentifier))
		return true;

	// ... if not we're different
	return false;
}

bool
FHoudiniOutputObjectIdentifier::Matches(const FHoudiniGeoPartObject& InHGPO) const
{
	// Object/Geo/Part IDs must match
	bool bMatchingIds = true;
	if (ObjectId != InHGPO.ObjectId
		|| GeoId != InHGPO.GeoId
		|| PartId != InHGPO.PartId)
		bMatchingIds = false;

	if ((bLoaded && !InHGPO.bLoaded) || (!bLoaded && InHGPO.bLoaded))
	{
		// If either the HGPO or the Identifer is nmarked as loaded, 
		// we can simply compare the part names
		if (PartName.Equals(InHGPO.PartName))
			return true;
	}

	if (!bMatchingIds)
	{
		return false;
	}

	// If the HGPO has our split identifier
	//if (InHGPO.SplitGroups.Contains(SplitIdentifier))
	//	return true;

	//
	return true;
}


// ----------------------------------------------------
// FHoudiniBakedOutputObjectIdentifier
// ----------------------------------------------------


FHoudiniBakedOutputObjectIdentifier::FHoudiniBakedOutputObjectIdentifier()
{
	PartId = -1;
	SplitIdentifier = FString();
}

FHoudiniBakedOutputObjectIdentifier::FHoudiniBakedOutputObjectIdentifier(
	const int32& InPartId, const FString& InSplitIdentifier)
{
	PartId = InPartId;
	SplitIdentifier = InSplitIdentifier;
}

FHoudiniBakedOutputObjectIdentifier::FHoudiniBakedOutputObjectIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	PartId = InIdentifier.PartId;
	SplitIdentifier = InIdentifier.SplitIdentifier;
}

uint32
FHoudiniBakedOutputObjectIdentifier::GetTypeHash() const
{
	const int32 HashBuffer = PartId;
	const int32 Hash = FCrc::MemCrc32((void *)&HashBuffer, sizeof(HashBuffer));
	return FCrc::StrCrc32(*SplitIdentifier, Hash);
}

uint32
GetTypeHash(const FHoudiniBakedOutputObjectIdentifier& InIdentifier)
{
	return InIdentifier.GetTypeHash();
}

bool
FHoudiniBakedOutputObjectIdentifier::operator==(const FHoudiniBakedOutputObjectIdentifier& InIdentifier) const
{
	return (InIdentifier.PartId == PartId && InIdentifier.SplitIdentifier.Equals(SplitIdentifier)); 
}


// ----------------------------------------------------
// FHoudiniBakedOutputObject
// ----------------------------------------------------


FHoudiniBakedOutputObject::FHoudiniBakedOutputObject()
	: Actor()
	, ActorBakeName(NAME_None)
	, BakedObject()
	, BakedComponent()
	, BakedSkeleton()
{
}


FHoudiniBakedOutputObject::FHoudiniBakedOutputObject(AActor* InActor, FName InActorBakeName, UObject* InBakeObject, UObject* InBakedComponent)
	: Actor(FSoftObjectPath(InActor).ToString())
	, ActorBakeName(InActorBakeName)
	, BakedObject(FSoftObjectPath(InBakeObject).ToString())
	, BakedComponent(FSoftObjectPath(InBakedComponent).ToString())
	, BakedSkeleton()
{
}


AActor*
FHoudiniBakedOutputObject::GetActorIfValid(bool bInTryLoad) const
{
	const FSoftObjectPath ActorPath(Actor);
	
	if (!ActorPath.IsValid())
		return nullptr;
	
	UObject* Object = ActorPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = ActorPath.TryLoad();
	
	if (!IsValid(Object))
		return nullptr;
	
	return Cast<AActor>(Object);
}

UObject*
FHoudiniBakedOutputObject::GetBakedObjectIfValid(bool bInTryLoad) const 
{ 
	const FSoftObjectPath ObjectPath(BakedObject);

	if (!ObjectPath.IsValid())
		return nullptr;
	
	UObject* Object = ObjectPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = ObjectPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Object;
}

UObject*
FHoudiniBakedOutputObject::GetBakedComponentIfValid(bool bInTryLoad) const 
{ 
	const FSoftObjectPath ComponentPath(BakedComponent);

	if (!ComponentPath.IsValid())
		return nullptr;
	
	UObject* Object = ComponentPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = ComponentPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Object;
}

UBlueprint*
FHoudiniBakedOutputObject::GetBlueprintIfValid(bool bInTryLoad) const 
{ 
	const FSoftObjectPath BlueprintPath(Blueprint);

	if (!BlueprintPath.IsValid())
		return nullptr;
	
	UObject* Object = BlueprintPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = BlueprintPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Cast<UBlueprint>(Object);
}

ULandscapeLayerInfoObject*
FHoudiniBakedOutputObject::GetLandscapeLayerInfoIfValid(const FName& InLayerName, const bool bInTryLoad) const
{
	if (!LandscapeLayers.Contains(InLayerName))
		return nullptr;
	
	const FString& LayerInfoPathStr = LandscapeLayers.FindChecked(InLayerName);
	const FSoftObjectPath LayerInfoPath(LayerInfoPathStr);

	if (!LayerInfoPath.IsValid())
		return nullptr;
	
	UObject* Object = LayerInfoPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = LayerInfoPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Cast<ULandscapeLayerInfoObject>(Object);
}

ALandscape*
FHoudiniBakedOutputObject::GetLandscapeIfValid(bool bInTryLoad) const
{
    const FSoftObjectPath LandscapePath(Landscape);

    if (!LandscapePath.IsValid())
        return nullptr;

    UObject* Object = LandscapePath.ResolveObject();
    if (!Object && bInTryLoad)
        Object = LandscapePath.TryLoad();

    if (!IsValid(Object))
        return nullptr;

    return Cast<ALandscape>(Object);
}

USkeleton*
FHoudiniBakedOutputObject::GetBakedSkeletonIfValid(bool bInTryLoad) const
{
	const FSoftObjectPath SkeletonPath(BakedSkeleton);

	if (!SkeletonPath.IsValid())
		return nullptr;
	
	UObject* Object = SkeletonPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = SkeletonPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Cast<USkeleton>(Object);
}

UPhysicsAsset*
FHoudiniBakedOutputObject::GetBakedPhysicsAssetIfValid(bool bInTryLoad) const
{
	const FSoftObjectPath PhyscsAssetPath(BakedPhysicsAsset);

	if (!PhyscsAssetPath.IsValid())
		return nullptr;

	UObject* Object = PhyscsAssetPath.ResolveObject();
	if (!Object && bInTryLoad)
		Object = PhyscsAssetPath.TryLoad();

	if (!IsValid(Object))
		return nullptr;

	return Cast<UPhysicsAsset>(Object);
}

TArray<AActor*>
FHoudiniBakedOutputObject::GetFoliageActorsIfValid(bool bInTryLoad) const
{
	TArray<AActor*> ValidActors;

    for (const FString& ActorPathString : FoliageActors)
    {
        FSoftObjectPath ActorPath(ActorPathString);

        if (!ActorPath.IsValid())
            continue;

        UObject* ResolvedObject = ActorPath.ResolveObject();
        if (!ResolvedObject && bInTryLoad)
            ResolvedObject = ActorPath.TryLoad();

        if (!IsValid(ResolvedObject))
            continue;

        AActor* ResolvedActor = Cast<AActor>(ResolvedObject);
        if (ResolvedActor)
        {
            ValidActors.Add(ResolvedActor);
        }
    }

    return ValidActors;
}

TArray<AActor*> FHoudiniBakedOutputObject::GetInstancedActorsIfValid(bool bInTryLoad) const
{
    TArray<AActor*> ValidActors;

    for (const FString& ActorPathString : InstancedActors)
    {
        FSoftObjectPath ActorPath(ActorPathString);

        if (!ActorPath.IsValid())
            continue;

        UObject* ResolvedObject = ActorPath.ResolveObject();
        if (!ResolvedObject && bInTryLoad)
            ResolvedObject = ActorPath.TryLoad();

        if (!IsValid(ResolvedObject))
            continue;

        AActor* ResolvedActor = Cast<AActor>(ResolvedObject);
        if (ResolvedActor)
        {
            ValidActors.Add(ResolvedActor);
        }
    }

    return ValidActors;
}

UHoudiniOutput::UHoudiniOutput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EHoudiniOutputType::Invalid)
	, StaleCount(0)
	, bCreateSceneComponents(true)
	, bLandscapeWorldComposition(false)
	, bIsEditableNode(false)
	, bHasEditableNodeBuilt(false)
	, bCanDeleteHoudiniNodes(true)
{
	
}

UHoudiniOutput::~UHoudiniOutput()
{
	Type = EHoudiniOutputType::Invalid;
	StaleCount = 0;
	bIsUpdating = false;

	HoudiniGeoPartObjects.Empty();
	OutputObjects.Empty();
	InstancedOutputs.Empty();
	AssignmentMaterialsById.Empty();
	ReplacementMaterialsById.Empty();
}

void
UHoudiniOutput::BeginDestroy()
{
	Super::BeginDestroy();
}

void
UHoudiniOutput::PostLoad()
{
	Super::PostLoad();
}


FBox 
UHoudiniOutput::GetBounds() const 
{
	FBox BoxBounds(ForceInitToZero);

	switch (GetType())
	{
	case EHoudiniOutputType::Mesh:
	{
		for (auto & CurPair : OutputObjects)
		{
			const FHoudiniOutputObject& CurObj = CurPair.Value;

			if (CurObj.bProxyIsCurrent)
			{
				UMeshComponent* MeshComp = Cast<UMeshComponent>(CurObj.ProxyComponent);
				if (!IsValid(MeshComp))
					continue;

				BoxBounds += MeshComp->Bounds.GetBox();
			}
			else
			{
				for(auto Component : CurObj.OutputComponents)
				{
					UMeshComponent* MeshComp = Cast<UMeshComponent>(Component);
					if (!IsValid(MeshComp))
						continue;

					BoxBounds += MeshComp->Bounds.GetBox();
				}
			}
		}
	}
	break;

	case EHoudiniOutputType::Landscape:
	{
		for (auto & CurPair : OutputObjects)
		{
			const FHoudiniOutputObject& CurObj = CurPair.Value;
			UHoudiniLandscapePtr* CurLandscapeObj = Cast<UHoudiniLandscapePtr>(CurObj.OutputObject);
			if (!IsValid(CurLandscapeObj))
				continue;

			ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(CurLandscapeObj->GetRawPtr());
			if (!IsValid(Landscape))
				continue;

			FVector Origin, Extent;
			Landscape->GetActorBounds(false, Origin, Extent);

			FBox LandscapeBounds = FBox::BuildAABB(Origin, Extent);
			BoxBounds += LandscapeBounds;
		}
	}
	break;

	case EHoudiniOutputType::Instancer:
	{
		for (auto & CurPair : OutputObjects)
		{
			const FHoudiniOutputObject& CurObj = CurPair.Value;
			USceneComponent* InstancedComp = Cast<USceneComponent>(CurObj.OutputObject);
			if (!IsValid(InstancedComp))
				continue;

			BoxBounds += InstancedComp->Bounds.GetBox();
		}
	}
	break;

	case EHoudiniOutputType::Curve:
	{
		for (auto & CurPair : OutputObjects)
		{
			const FHoudiniOutputObject& CurObj = CurPair.Value;
			for(auto Component : CurObj.OutputComponents)
			{
			    UHoudiniSplineComponent* CurHoudiniSplineComp = Cast<UHoudiniSplineComponent>(Component);
			    if (!IsValid(CurHoudiniSplineComp))
				    continue;

			    FBox CurCurveBound(ForceInitToZero);
			    for (auto & Trans : CurHoudiniSplineComp->CurvePoints)
			    {
				    CurCurveBound += Trans.GetLocation();
			    }

				UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(GetOuter());
				if (IsValid(OuterHC))
				{
					USceneComponent* OuterComp = OuterHC->GetComponent();
					if (IsValid(OuterComp))
						BoxBounds += CurCurveBound.MoveTo(OuterComp->GetComponentLocation());
				}
			}
		}
	}
	break;

	case EHoudiniOutputType::LandscapeSpline:
	{
		for (const auto& CurPair : OutputObjects)
		{
			const FHoudiniOutputObject& CurObj = CurPair.Value;
			ALandscapeSplineActor* CurLandscapeSpline = Cast<ALandscapeSplineActor>(CurObj.OutputObject);
			if (!IsValid(CurLandscapeSpline))
				continue;

			FVector Origin, Extent;
			CurLandscapeSpline->GetActorBounds(false, Origin, Extent);

			FBox LandscapeBounds = FBox::BuildAABB(Origin, Extent);
			BoxBounds += LandscapeBounds;
		}
		break;
	}

	case EHoudiniOutputType::Skeletal:
	case EHoudiniOutputType::Invalid:
		break;

	default:
		break;
	}

	return BoxBounds;
}

void
UHoudiniOutput::Clear()
{
	StaleCount = 0;

	HoudiniGeoPartObjects.Empty();

	for (auto& CurrentOutputObject : OutputObjects)
	{
		for (auto Component : CurrentOutputObject.Value.OutputComponents)
		{
		    UHoudiniSplineComponent* SplineComponent = Cast<UHoudiniSplineComponent>(Component);
		    if (IsValid(SplineComponent))
		    {
			    // The spline component is a special case where the output
			    // object as associated Houdini nodes (as input object).
			    // We can only explicitly remove those nodes when the output object gets
			    // removed. 
			    SplineComponent->MarkInputNodesAsPendingKill();
		    }
		    
		    // Clear the output component
		    USceneComponent* SceneComp = Cast<USceneComponent>(Component);
		    if (IsValid(SceneComp))
		    {
				if(SceneComp->GetOwner())
					SceneComp->UnregisterComponent();

			    SceneComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);			    
			    SceneComp->DestroyComponent();
		    }

		    if (Type == EHoudiniOutputType::Landscape && !bLandscapeWorldComposition && !IsGarbageCollecting())
		    {
			    // NOTE: We cannot resolve soft pointers during garbage collection. Any Get() or IsValid() call
			    // will result in a StaticFindObject() call which will raise an exception during GC.
			    UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(CurrentOutputObject.Value.OutputObject);
			    TSoftObjectPtr<ALandscapeProxy> LandscapeProxy = LandscapePtr ? LandscapePtr->GetSoftPtr() : nullptr;
			    if (!LandscapeProxy.IsNull() && LandscapeProxy.IsValid())
			    {
				    LandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				    LandscapeProxy->ConditionalBeginDestroy();
					FHoudiniLandscapeRuntimeUtils::DestroyLandscapeProxy(LandscapeProxy.Get());
				    LandscapePtr->SetSoftPtr(nullptr);
			    }
		    }
		}

		// Also destroy proxy components
		USceneComponent* ProxyComp = Cast<USceneComponent>(CurrentOutputObject.Value.ProxyComponent);
		if (IsValid(ProxyComp))
		{
			if (ProxyComp->GetOwner())
				ProxyComp->UnregisterComponent();

			ProxyComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			ProxyComp->DestroyComponent();
		}

		// Destroy Landscape Spline Output Object
		if (IsValid(CurrentOutputObject.Value.OutputObject) && CurrentOutputObject.Value.OutputObject->IsA<UHoudiniLandscapeSplinesOutput>())
		{
			UHoudiniLandscapeSplinesOutput* const LandscapeSplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(CurrentOutputObject.Value.OutputObject);
			LandscapeSplinesOutputObject->Clear();
		}
	}

	OutputObjects.Empty();
	InstancedOutputs.Empty();
	AssignmentMaterialsById.Empty();
	ReplacementMaterialsById.Empty();

	Type = EHoudiniOutputType::Invalid;
}

bool 
UHoudiniOutput::ShouldDeferClear() const
{
	if (Type == EHoudiniOutputType::Landscape)
		return true;

	return false;
}

const bool 
UHoudiniOutput::HasGeoChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasGeoChanged)
			return true;
	}

	return false;
}

const bool
UHoudiniOutput::HasTransformChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasTransformChanged)
			return true;
	}

	return false;
}

const bool
UHoudiniOutput::HasMaterialsChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasMaterialsChanged)
			return true;
	}

	return false;
}

const bool 
UHoudiniOutput::HasHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHGPO) const
{
	return HoudiniGeoPartObjects.Find(InHGPO) != INDEX_NONE;
}

const bool
UHoudiniOutput::HeightfieldMatch(const FHoudiniGeoPartObject& InHGPO, const bool& bVolumeNameShouldMatch) const
{	
	if (InHGPO.Type != EHoudiniPartType::Volume)
		return false;

	if (InHGPO.VolumeName.IsEmpty())
		return false;

	for (auto& currentHGPO : HoudiniGeoPartObjects)
	{
		// Asset/Object/Geo IDs should match
		if (currentHGPO.AssetId != InHGPO.AssetId
			|| currentHGPO.ObjectId != InHGPO.ObjectId
			|| currentHGPO.GeoId != InHGPO.GeoId)
		{
			continue;
		}

		// Both HGPO type should be volumes
		if (currentHGPO.Type != EHoudiniPartType::Volume)
		{
			continue;
		}

		// Volume tile index should match
		if (currentHGPO.VolumeTileIndex != InHGPO.VolumeTileIndex)
		{
			continue;
		}

		// We've specified if we want the name to match/to be different:
		// when looking in previous outputs, we want the name to match
		// when looking in newly created outputs, we want to be sure the names are different
		if (bVolumeNameShouldMatch)
		{
			// HasEditLayers state should match.
			if (!(InHGPO.bHasEditLayers == currentHGPO.bHasEditLayers))
			{
				continue;
			}
			
			// If we have edit layers, ensure the names match
			if (InHGPO.bHasEditLayers && !InHGPO.VolumeLayerName.Equals(currentHGPO.VolumeLayerName, ESearchCase::IgnoreCase))
			{
				continue;
			}

			// Check whether the volume name match.
			if (!(InHGPO.VolumeName.Equals(currentHGPO.VolumeName, ESearchCase::IgnoreCase)))
			{
				continue;
			}
		}

		return true;
	}

	return false;
}

const bool UHoudiniOutput::GeoMatch(const FHoudiniGeoPartObject& InHGPO) const
{
	for (auto& currentHGPO : HoudiniGeoPartObjects)
	{
		// Asset/Object/Geo IDs should match
		if (currentHGPO.AssetId != InHGPO.AssetId
			|| currentHGPO.ObjectId != InHGPO.ObjectId
			|| currentHGPO.GeoId != InHGPO.GeoId)
		{
			continue;
		}

		return true;
	}

	return false;
}

const bool UHoudiniOutput::InstancerNameMatch(const FHoudiniGeoPartObject& InHGPO) const
{
	for (auto& currentHGPO : HoudiniGeoPartObjects)
	{
		// Asset/Object/Geo IDs should match
		if (currentHGPO.AssetId != InHGPO.AssetId
			|| currentHGPO.ObjectId != InHGPO.ObjectId
			|| currentHGPO.GeoId != InHGPO.GeoId
			|| currentHGPO.InstancerName != InHGPO.InstancerName
			)
		{
			continue;
		}

		return true;
	}

	return false;
}

void 
UHoudiniOutput::MarkAllHGPOsAsStale(const bool& bInStale)
{
	// Since objects can only be added to this array,
	// Simply keep track of the current number of HoudiniGeoPartObject
	StaleCount = bInStale ? HoudiniGeoPartObjects.Num() : 0;
}

void 
UHoudiniOutput::DeleteAllStaleHGPOs()
{
	// Simply delete the first "StaleCount" objects and reset the stale marker
	HoudiniGeoPartObjects.RemoveAt(0, StaleCount);
	StaleCount = 0;
}

void 
UHoudiniOutput::AddNewHGPO(const FHoudiniGeoPartObject& InHGPO)
{
	HoudiniGeoPartObjects.Add(InHGPO);
}

void
UHoudiniOutput::UpdateOutputType()
{
	int32 MeshCount = 0;
	int32 CurveCount = 0;
	int32 VolumeCount = 0;
	int32 InstancerCount = 0;
	int32 DataTableCount = 0;
	int32 LandscapeSplineCount = 0;
	int32 AnimSequenceCount = 0;
	int32 SkeletonCount = 0;
	int32 CopCount = 0;
	int32 PCGCount = 0;

	for (auto& HGPO : HoudiniGeoPartObjects)
	{
		switch (HGPO.Type)
		{
		case EHoudiniPartType::Mesh:
			MeshCount++;
			break;
		case EHoudiniPartType::Curve:
			CurveCount++;
			break;
		case EHoudiniPartType::Volume:
			VolumeCount++;
			break;
		case EHoudiniPartType::Instancer:
			InstancerCount++;
			break;
		case EHoudiniPartType::DataTable:
			DataTableCount++;
			break;
		case EHoudiniPartType::LandscapeSpline:
			LandscapeSplineCount++;
			break;
		case EHoudiniPartType::MotionClip:
			AnimSequenceCount++;
			break;
		case EHoudiniPartType::SkeletalMeshPose:
			SkeletonCount++;
			break;
		case EHoudiniPartType::SkeletalMeshShape:
			SkeletonCount++;
			break;
		case EHoudiniPartType::Cop:
			CopCount++;
			break;
		case EHoudiniPartType::PCG:
			PCGCount++;
		default:
		case EHoudiniPartType::Invalid:
			break;
		}
	}
	
	if (VolumeCount > 0)
	{
		// If we have a volume, we're a landscape
		Type = EHoudiniOutputType::Landscape;
	}
	else if (AnimSequenceCount > 0)
	{
		// Anim sequence take precedence over instancers and meshes since it contains both
		Type = EHoudiniOutputType::AnimSequence;
	}
	else if (SkeletonCount > 0)
	{
		// Skeletal Meshes take precedence over instancers and meshes since it contains both
		Type = EHoudiniOutputType::Skeletal;
	}
	else if (InstancerCount > 0)
	{
		// if we have at least an instancer, we're one
		Type = EHoudiniOutputType::Instancer;
	}
	else if (MeshCount > 0)
	{
		Type = EHoudiniOutputType::Mesh;
	}
	else if (CurveCount > 0)
	{
		Type = EHoudiniOutputType::Curve;
	}
	else if (Type == EHoudiniOutputType::GeometryCollection)
	{
		// Geometry collections don't rely on HoudiniGeoPartObjects for construction, so keep the same Type.
	}
	else if (DataTableCount > 0)
	{
		Type = EHoudiniOutputType::DataTable;
	}
	else if (LandscapeSplineCount > 0)
	{
		Type = EHoudiniOutputType::LandscapeSpline;
	}
	else if (CopCount > 0)
	{
		Type = EHoudiniOutputType::Cop;
	}
	else if (PCGCount > 0)
	{
		Type = EHoudiniOutputType::PCG;
	}
	else
	{
		// No valid HGPO detected...
		Type = EHoudiniOutputType::Invalid;
	}
}

UHoudiniOutput*
UHoudiniOutput::DuplicateAndCopyProperties(UObject* DestOuter, FName NewName)
{
	UHoudiniOutput* NewOutput = Cast<UHoudiniOutput>(StaticDuplicateObject(this, DestOuter, NewName));

	NewOutput->CopyPropertiesFrom(this, false);

	return NewOutput;
}

void
UHoudiniOutput::CopyPropertiesFrom(UHoudiniOutput* InInput, bool bCopyAllProperties)
{
	// Copy the state of this UHoudiniInput object.
	if (bCopyAllProperties)
	{
		// Stash all the data that we want to preserve, and re-apply after property copy took place
		// (similar to Get/Apply component instance data). This is typically only needed
		// for certain properties that require cleanup when being replaced / removed.
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> PrevOutputObjects = OutputObjects;
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput> PrevInstancedOutputs = InstancedOutputs;

		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References will be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InInput, this, Params);

		// Restore the desired properties.
		OutputObjects = PrevOutputObjects;
		InstancedOutputs = PrevInstancedOutputs;
	}

	// Copy any additional DuplicateTransient properties.
	bHasEditableNodeBuilt = InInput->bHasEditableNodeBuilt;
}

void
UHoudiniOutput::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	bCanDeleteHoudiniNodes = bInCanDeleteNodes;
}

FString
UHoudiniOutput::OutputTypeToString(const EHoudiniOutputType& InOutputType)
{
	FString OutputTypeStr;
	switch (InOutputType)
	{
		case EHoudiniOutputType::Mesh:
			OutputTypeStr = TEXT("Mesh");
			break;
		case EHoudiniOutputType::Instancer:
			OutputTypeStr = TEXT("Instancer");
			break;
		case EHoudiniOutputType::Landscape:
			OutputTypeStr = TEXT("Landscape");
			break;
		case EHoudiniOutputType::Curve:
			OutputTypeStr = TEXT("Curve");
			break;
		case EHoudiniOutputType::Skeletal:
			OutputTypeStr = TEXT("Skeletal");
			break;
		case EHoudiniOutputType::GeometryCollection:
			OutputTypeStr = TEXT("GeometryCollection");
			break;
		case EHoudiniOutputType::DataTable:
			OutputTypeStr = TEXT("DataTable");
			break;
		case EHoudiniOutputType::LandscapeSpline:
			OutputTypeStr = TEXT("LandscapeSpline");
			break;
		case EHoudiniOutputType::AnimSequence:
			OutputTypeStr = TEXT("AnimSequence");
			break;
		case EHoudiniOutputType::Cop:
			OutputTypeStr = TEXT("Cop");
			break;
		case EHoudiniOutputType::PCG:
			OutputTypeStr = TEXT("PCG");
			break;
		default:
		case EHoudiniOutputType::Invalid:
			OutputTypeStr = TEXT("Invalid");
			break;
	}

	return OutputTypeStr;
}

void
UHoudiniOutput::MarkAsLoaded(const bool& InLoaded)
{
	// Mark all HGPO as loaded
	for (auto& HGPO : HoudiniGeoPartObjects)
	{
		HGPO.bLoaded = InLoaded;
	}

	// Mark all output object's identifier as loaded
	for (auto& Iter : OutputObjects)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Iter.Key;
		Identifier.bLoaded = InLoaded;
	}

	// Instanced outputs
	for (auto& Iter : InstancedOutputs)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Iter.Key;
		Identifier.bLoaded = InLoaded;
	}
}


const bool 
UHoudiniOutput::HasAnyProxy() const
{
	for (const auto& Pair : OutputObjects)
	{
		UObject* FoundProxy = Pair.Value.ProxyObject;
		if (IsValid(FoundProxy))
		{
			return true;
		}
	}

	return false;
}

const bool 
UHoudiniOutput::HasProxy(const FHoudiniOutputObjectIdentifier& InIdentifier) const
{
	const FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);
	if (!FoundOutputObject)
		return false;

	UObject* FoundProxy = FoundOutputObject->ProxyObject;
	if (!IsValid(FoundProxy))
		return false;

	return true;
}

const bool
UHoudiniOutput::HasAnyCurrentProxy() const
{
	for (const auto& Pair : OutputObjects)
	{
		UObject* FoundProxy = Pair.Value.ProxyObject;
		if (IsValid(FoundProxy))
		{
			if(Pair.Value.bProxyIsCurrent)
			{
				return true;
			}
		}
	}

	return false;
}

const bool 
UHoudiniOutput::IsProxyCurrent(const FHoudiniOutputObjectIdentifier &InIdentifier) const
{
	if (!HasProxy(InIdentifier))
		return false;

	const FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);
	if (!FoundOutputObject)
		return false;

	return FoundOutputObject->bProxyIsCurrent;
}


void FHoudiniClearedEditLayers::Empty()
{
	EditLayers.Empty();
}


bool FHoudiniClearedEditLayers::Contains(FString& EditLayer, FString& TargetLayer)
{
	return EditLayers.Contains(EditLayer) && EditLayers[EditLayer].TargetLayers.Contains(TargetLayer);
}

void FHoudiniClearedEditLayers::Add(FString& EditLayer, FString& TargetLayer)
{
	if (!EditLayers.Contains(EditLayer))
		EditLayers.Add(EditLayer, {});

	EditLayers[EditLayer].TargetLayers.Add(TargetLayer);
};

void DestroyComponent(UObject * Component)
{
	if (Component == nullptr || !IsValid(Component))
		return;

	USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
	if (IsValid(SceneComponent))
	{
		// Remove from the HoudiniAssetActor
		if (SceneComponent->GetOwner())
		{
			SceneComponent->GetOwner()->RemoveOwnedComponent(SceneComponent);
			SceneComponent->UnregisterComponent();
		}

		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SceneComponent->DestroyComponent();
	}
}

void FHoudiniOutputObject::DestroyCookedData(EHoudiniClearFlags ClearFlags)
{
	//--------------------------------------------------------------------------------------------------------------------
	// Destroy all components
	//--------------------------------------------------------------------------------------------------------------------
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputObject::DestroyCookedData);

	TArray<UObject*> ComponentsToDestroy;

	for (auto Component : OutputComponents)
	{
		if (IsValid(Component))
		{
			ComponentsToDestroy.Add(Component);
		}
	}
	OutputComponents.Empty();

	if (IsValid(ProxyComponent))
		ComponentsToDestroy.Add(ProxyComponent);

	ProxyComponent = nullptr;

	for(UObject* Component : ComponentsToDestroy)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		if(SceneComponent->GetOwner())
			SceneComponent->UnregisterComponent();

		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);		
		SceneComponent->DestroyComponent();
	}


	//--------------------------------------------------------------------------------------------------------------------
	// Delete output packages, if they exist
	//--------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

	bool bClearAssets = (ClearFlags & EHoudiniClearFlags::EHoudiniClear_Assets) == EHoudiniClearFlags::EHoudiniClear_Assets;

	if(bClearAssets && IsValid(OutputObject))
	{
		TArray<FString> PackagesDeleted;

		bool bCanDelete = false;
		if(OutputObject->IsA<UStaticMesh>() ||
			OutputObject->IsA<UMaterial>())
		{
			bCanDelete = true;
		}

		if (bCanDelete)
		{
			if(UPackage* Package = OutputObject->GetPackage())
			{
				TArray<UObject*> ObjectsToDelete;
				ObjectsToDelete.Add(Package);
				GetObjectsWithOuter(Package, ObjectsToDelete, true);

				// Use ObjectTools to delete
				ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);

				// Also delete the package file from disk
				FString PackagePath = Package->GetPathName();
				FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

				FString DirectoryPath = FPaths::GetPath(FilePath);

				TArray<FString> Files;
				TArray<FString> SubDirs;
				IFileManager::Get().FindFiles(Files, *(DirectoryPath / TEXT("*")), true, false);
				IFileManager::Get().FindFiles(SubDirs, *(DirectoryPath / TEXT("*")), false, true);

				if(Files.IsEmpty() && SubDirs.IsEmpty())
				{
					IFileManager::Get().DeleteDirectory(*DirectoryPath, false, true);
				}
			}
		}
	}

	bool bClearLandscapeLayers = (ClearFlags & EHoudiniClearFlags::EHoudiniClear_LandscapeLayers) == EHoudiniClearFlags::EHoudiniClear_LandscapeLayers;
	UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject);
	
	if (bClearLandscapeLayers && IsValid(LayerOutput))
	{
		// For now, only delete layers we created.
		if (LayerOutput->bLayerWasCreated)
			FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(LayerOutput->Landscape, FName(LayerOutput->CookedEditLayer));
	}

#endif

	//--------------------------------------------------------------------------------------------------------------------
	// Remove spline output
	//--------------------------------------------------------------------------------------------------------------------

	// Destroy any segments that we previously created
	UHoudiniLandscapeSplinesOutput* SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(this->OutputObject);
	if (IsValid(SplinesOutputObject))
	{
		SplinesOutputObject->Clear();
	}

	//--------------------------------------------------------------------------------------------------------------------
	// If we overwrite a package that contains a data table, Unreal can assert if it points an old RowStruct. So
	// a workaround is to point the Data Table to a temp structure before its deleted.
	//--------------------------------------------------------------------------------------------------------------------

	if(UDataTable* Table = Cast<UDataTable>(OutputObject.Get()))
	{
		if (Table->RowStruct)
		{
			Table->RowStruct = Cast<UScriptStruct>(StaticDuplicateObject(Table->RowStruct, GetTransientPackage()));
			Table->RowStruct->SetFlags(RF_Transient);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Destroy all objects
	//--------------------------------------------------------------------------------------------------------------------

	if (UHoudiniLandscapeTargetLayerOutput * LandscapeOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject))
	{
		// We can only clean up new landscapes. Modifications to existing landscapes are destructive, so not much
		// we can do...

		if (LandscapeOutput->bCreatedLandscape)
		{
			if(IsValid(LandscapeOutput->Landscape))
			{
				LandscapeOutput->Landscape->Destroy();
				LandscapeOutput->Landscape = nullptr;
			}
		}
	}
	else if (IsValid(OutputObject))
	{
#if WITH_EDITOR
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		AssetEditorSubsystem->CloseAllEditorsForAsset(OutputObject);
#endif
	}
	OutputObject = nullptr;

	ProxyObject = nullptr;

	//--------------------------------------------------------------------------------------------------------------------
	// Remove Foliage creating during cooking; we just need to move it from the world, and all the instances will be deleted.
	//--------------------------------------------------------------------------------------------------------------------

	if (IsValid(FoliageType))
	{
		FHoudiniFoliageUtils::RemoveFoliageTypeFromWorld(World, FoliageType);
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Remove actors
	//--------------------------------------------------------------------------------------------------------------------

	for (auto Actor : OutputActors)
	{
		if (Actor.IsValid())
		{
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->Destroy();
		}
	}
	OutputActors.Empty();

}


void UHoudiniOutput::DestroyCookedData(EHoudiniClearFlags ClearFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHoudiniOutput::DestroyCookedData);
	for (auto It : OutputObjects)
	{
		FHoudiniOutputObject* FoundOutputObject = &It.Value;
		FoundOutputObject->DestroyCookedData(ClearFlags);
	}
	OutputObjects.Empty();
}

