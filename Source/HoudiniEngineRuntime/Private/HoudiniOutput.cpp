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

#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniSplineComponent.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Misc/StringFormatArg.h"
#include "Engine/Engine.h"
#include "LandscapeLayerInfoObject.h"

UHoudiniLandscapePtr::UHoudiniLandscapePtr(class FObjectInitializer const& Initializer) 
{
	// bIsWorldCompositionLandscape = false;
	// BakeType = EHoudiniLandscapeOutputBakeType::Detachment;
};

uint32
GetTypeHash(const FHoudiniOutputObjectIdentifier& HoudiniOutputObjectIdentifier)
{
	return HoudiniOutputObjectIdentifier.GetTypeHash();
}

void
FHoudiniInstancedOutput::SetVariationObjectAt(const int32& AtIndex, UObject* InObject)
{
	// Resize the array if needed
	if (VariationObjects.Num() <= AtIndex)
		VariationObjects.SetNum(AtIndex + 1);

	if (VariationTransformOffsets.Num() <= AtIndex)
		VariationTransformOffsets.SetNum(AtIndex + 1);

	UObject* CurrentObject = VariationObjects[AtIndex].LoadSynchronous();
	if (CurrentObject == InObject)
		return;

	VariationObjects[AtIndex] = InObject;
}

bool 
FHoudiniInstancedOutput::SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = VariationTransformOffsets.IsValidIndex(AtIndex) ? &VariationTransformOffsets[AtIndex] : nullptr;
	if (!Transform)
		return false;

	if (PosRotScaleIndex == 0)
	{
		FVector Position = Transform->GetLocation();
		if (Position[XYZIndex] == Value)
			return false;
		Position[XYZIndex] = Value;
		Transform->SetLocation(Position);
	}
	else if (PosRotScaleIndex == 1)
	{
		FRotator Rotator = Transform->Rotator();
		switch (XYZIndex)
		{
		case 0:
		{
			if (Rotator.Roll == Value)
				return false;
			Rotator.Roll = Value;
			break;
		}

		case 1:
		{
			if (Rotator.Pitch == Value)
				return false;
			Rotator.Pitch = Value;
			break;
		}

		case 2:
		{
			if (Rotator.Yaw == Value)
				return false;
			Rotator.Yaw = Value;
			break;
		}
		}
		Transform->SetRotation(Rotator.Quaternion());
	}
	else if (PosRotScaleIndex == 2)
	{
		FVector Scale = Transform->GetScale3D();
		if (Scale[XYZIndex] == Value)
			return false;

		Scale[XYZIndex] = Value;
		Transform->SetScale3D(Scale);
	}

	MarkChanged(true);

	return true;
}

float 
FHoudiniInstancedOutput::GetTransformOffsetAt(const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = VariationTransformOffsets.IsValidIndex(AtIndex) ? &VariationTransformOffsets[AtIndex] : nullptr;
	if (!Transform)
		return 0.0f;

	if (PosRotScaleIndex == 0)
	{
		FVector Position = Transform->GetLocation();
		return Position[XYZIndex];
	}
	else if (PosRotScaleIndex == 1)
	{
		FRotator Rotator = Transform->Rotator();
		switch (XYZIndex)
		{
			case 0:
			{
				return Rotator.Roll;
			}

			case 1:
			{
				return Rotator.Pitch;
			}

			case 2:
			{
				return Rotator.Yaw;
			}
		}
	}
	else if (PosRotScaleIndex == 2)
	{
		FVector Scale = Transform->GetScale3D();
		return Scale[XYZIndex];
	}

	return 0.0f;
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
	const int32& InObjectId, const int32& InGeoId, const int32& InPartId, const FString& InSplitIdentifier)
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
{
}


FHoudiniBakedOutputObject::FHoudiniBakedOutputObject(AActor* InActor, FName InActorBakeName, UObject* InBakeObject, UObject* InBakedComponent)
	: Actor(FSoftObjectPath(InActor).ToString())
	, ActorBakeName(InActorBakeName)
	, BakedObject(FSoftObjectPath(InBakeObject).ToString())
	, BakedComponent(FSoftObjectPath(InBakedComponent).ToString())
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


UHoudiniOutput::UHoudiniOutput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EHoudiniOutputType::Invalid)
	, StaleCount(0)
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
	AssignementMaterials.Empty();
	ReplacementMaterials.Empty();
}

void
UHoudiniOutput::BeginDestroy()
{
	Super::BeginDestroy();
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

			UMeshComponent* MeshComp = nullptr;
			if (CurObj.bProxyIsCurrent)
			{
				MeshComp = Cast<UMeshComponent>(CurObj.ProxyComponent);
			}
			else
			{
				MeshComp = Cast<UMeshComponent>(CurObj.OutputComponent);
			}

			if (!IsValid(MeshComp))
				continue;

			BoxBounds += MeshComp->Bounds.GetBox();
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
			UHoudiniSplineComponent* CurHoudiniSplineComp = Cast<UHoudiniSplineComponent>(CurObj.OutputComponent);
			if (!IsValid(CurHoudiniSplineComp))
				continue;

			FBox CurCurveBound(ForceInitToZero);
			for (auto & Trans : CurHoudiniSplineComp->CurvePoints)
			{
				CurCurveBound += Trans.GetLocation();
			}

			UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
			if (IsValid(OuterHAC))
				BoxBounds += CurCurveBound.MoveTo(OuterHAC->GetComponentLocation());
		}

	}
	break;

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
		UHoudiniSplineComponent* SplineComponent = Cast<UHoudiniSplineComponent>(CurrentOutputObject.Value.OutputComponent);
		if (IsValid(SplineComponent))
		{
			// The spline component is a special case where the output
			// object as associated Houdini nodes (as input object).
			// We can only explicitly remove those nodes when the output object gets
			// removed. 
			SplineComponent->MarkInputNodesAsPendingKill();
		}
		
		// Clear the output component
		USceneComponent* SceneComp = Cast<USceneComponent>(CurrentOutputObject.Value.OutputComponent);
		if (IsValid(SceneComp))
		{
			SceneComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			SceneComp->UnregisterComponent();
			SceneComp->DestroyComponent();
		}


		// Also destroy proxy components
		USceneComponent* ProxyComp = Cast<USceneComponent>(CurrentOutputObject.Value.ProxyComponent);
		if (IsValid(ProxyComp))
		{
			ProxyComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			ProxyComp->UnregisterComponent();
			ProxyComp->DestroyComponent();
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
				LandscapeProxy->Destroy();
				LandscapePtr->SetSoftPtr(nullptr);
			}
		}
	}

	OutputObjects.Empty();
	InstancedOutputs.Empty();
	AssignementMaterials.Empty();
	ReplacementMaterials.Empty();

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
