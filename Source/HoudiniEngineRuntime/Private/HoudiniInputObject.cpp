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

#include "HoudiniInputTypes.h"
#include "HoudiniInputObject.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniInput.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeUtils.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/DataTable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Landscape.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "GameFramework/Volume.h"
#include "Camera/CameraComponent.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif

#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplinesComponent.h"
#include "Model.h"
#include "Engine/Brush.h"

#include "Kismet/KismetSystemLibrary.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#endif
#include "LevelInstance/LevelInstanceActor.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "EngineUtils.h"


//-----------------------------------------------------------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------------------------------------------------------

//
UHoudiniInputObject::UHoudiniInputObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EHoudiniInputObjectType::Invalid)
	, bHasChanged(false)
	, bNeedsToTriggerUpdate(false)
	, bTransformChanged(false)
	, MaterialReferences()
	, bCanDeleteHoudiniNodes(true)
	, bInputNodeHandleOverridesNodeIds(true)
	, Transform(FTransform::Identity)
	, InputNodeId(-1)
	, InputObjectNodeId(-1)
{
	Guid = FGuid::NewGuid();
}

//
UHoudiniInputStaticMesh::UHoudiniInputStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputSkeletalMesh::UHoudiniInputSkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniInputAnimation::UHoudiniInputAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputSkeletalMeshComponent::UHoudiniInputSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputGeometryCollection::UHoudiniInputGeometryCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputGeometryCollectionComponent::UHoudiniInputGeometryCollectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

FHoudiniLandscapeSplineControlPointData::FHoudiniLandscapeSplineControlPointData()
	: Location(FVector::ZeroVector)
	, Rotation(FRotator::ZeroRotator)
	, Width(1000.0f)
#if WITH_EDITORONLY_DATA
	, SegmentMeshOffset(0)
	, LayerName(NAME_None)
	, bRaiseTerrain(true)
	, bLowerTerrain(true)
	, Mesh(nullptr)
	, MeshScale(FVector::OneVector)
#endif
{
	
}

FHoudiniLandscapeSplineSegmentData::FHoudiniLandscapeSplineSegmentData()
#if WITH_EDITORONLY_DATA
	: LayerName(NAME_None)
	, bRaiseTerrain(true)
	, bLowerTerrain(true)
#endif
{
	
}

//
UHoudiniInputLandscapeSplineActor::UHoudiniInputLandscapeSplineActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputLandscapeSplinesComponent::UHoudiniInputLandscapeSplinesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NextControlPointId(0)
{

}

//
UHoudiniInputSceneComponent::UHoudiniInputSceneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputMeshComponent::UHoudiniInputMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputInstancedMeshComponent::UHoudiniInputInstancedMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputSplineComponent::UHoudiniInputSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NumberOfSplineControlPoints(-1)
	, SplineLength(-1.0f)
	, SplineResolution(-1.0f)
	, SplineClosed(false)
{

}

//
UHoudiniInputCameraComponent::UHoudiniInputCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FOV(0.0f)
	, AspectRatio(1.0f)
	, bIsOrthographic(false)
	, OrthoWidth(2.0f)
	, OrthoNearClipPlane(0.0f)
	, OrthoFarClipPlane(-1.0f)
{

}

// Return true if the component itself has been modified
bool 
UHoudiniInputSplineComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const 
{
	USplineComponent* SplineComponent = Cast<USplineComponent>(InputObject.LoadSynchronous());

	if (!SplineComponent)
		return false;

	if (SplineClosed != SplineComponent->IsClosedLoop()) 
		return true;

	if (SplineComponent->GetNumberOfSplinePoints() != NumberOfSplineControlPoints)
		return true;

	if (SplineComponent->GetSplineLength() != SplineLength)
		return true;

	for (int32 n = 0; n < SplineComponent->GetNumberOfSplinePoints(); ++n) 
	{
		const FTransform &CurSplineComponentTransform = SplineComponent->GetTransformAtSplinePoint(n, ESplineCoordinateSpace::Local, true);
		const FTransform &CurInputTransform = SplineControlPoints[n];

		if (!CurInputTransform.TranslationEquals(CurSplineComponentTransform))
			return true;

		if (!CurInputTransform.RotationEquals(CurSplineComponentTransform))
			return true;

		if (!CurInputTransform.Scale3DEquals(CurSplineComponentTransform))
			return true;
	}

	return false;
}


bool
UHoudiniInputLandscapeSplineActor::ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const
{
	// We only track ULandscapeSplinesComponents for landscape splines at this stage.
	if (!IsValid(InComponent))
		return false;

	// Use InSettings if provided, otherwise use the settings cached at the last update
	const FHoudiniInputObjectSettings& Settings = InSettings ? *InSettings : CachedInputSettings;
	
	if (InComponent->IsA(ULandscapeSplinesComponent::StaticClass()))
		return true;
		
	if (Settings.bLandscapeSplinesExportSplineMeshComponents && InComponent->IsA(USplineMeshComponent::StaticClass()))
		return true;
	
	return false;
}


// Return true if the component itself has been modified
bool 
UHoudiniInputLandscapeSplinesComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const 
{
	if (Super::HasComponentChanged(InSettings))
	{
		return true;
	}
	
	ULandscapeSplinesComponent* const SplinesComponent = Cast<ULandscapeSplinesComponent>(InputObject.LoadSynchronous());

	if (!IsValid(SplinesComponent))
		return false;

	// Get the control point and segment arrays and check if the counts have changed.
	// Use helper to fetch control points since the landscape splines API differs between UE 5.0 and 5.1+
	TArray<TObjectPtr<ULandscapeSplineControlPoint>> ControlPoints;
	TArray<TObjectPtr<ULandscapeSplineSegment>> Segments;
	FHoudiniEngineRuntimeUtils::GetLandscapeSplinesControlPointsAndSegments(SplinesComponent, &ControlPoints, &Segments);

	const int32 NumControlPoints = ControlPoints.Num();
	if (NumControlPoints != CachedControlPoints.Num())
	{
		return true;
	}
	const int32 NumSegments = Segments.Num();
	if (NumSegments != CachedSegments.Num())
	{
		return true;
	}

	// If the number of control points / segments remain the same, check each point / segment for differences
	if (NumControlPoints > 0)
	{
		for (int32 Idx = 0; Idx < NumControlPoints; ++Idx)
		{
			ULandscapeSplineControlPoint const* const ControlPoint = ControlPoints[Idx];
			const FHoudiniLandscapeSplineControlPointData& CachedControlPoint = CachedControlPoints[Idx];

			// Skip invalid points
			if (!IsValid(ControlPoint))
			{
				continue;
			}

			// Compare supported fields ...
			// TODO: could we simplify this by some kind of UProperty based diff, like UEdGraphNode does?
			if (ControlPoint->Location != CachedControlPoint.Location
					|| ControlPoint->Rotation != CachedControlPoint.Rotation
#if WITH_EDITORONLY_DATA
					|| ControlPoint->SegmentMeshOffset != CachedControlPoint.SegmentMeshOffset
					|| ControlPoint->LayerName != CachedControlPoint.LayerName
					|| ControlPoint->bRaiseTerrain != CachedControlPoint.bRaiseTerrain
					|| ControlPoint->bLowerTerrain != CachedControlPoint.bLowerTerrain
					|| ControlPoint->Mesh != CachedControlPoint.Mesh
					|| ControlPoint->MaterialOverrides != CachedControlPoint.MaterialOverrides
					|| ControlPoint->MeshScale != CachedControlPoint.MeshScale
#endif
					|| ControlPoint->Width != CachedControlPoint.Width)
			{
				return true;
			}
		}
	}
	
	if (NumSegments > 0)
	{
		for (int32 Idx = 0; Idx < NumSegments; ++Idx)
		{
			ULandscapeSplineSegment const* const Segment = Segments[Idx];
			const FHoudiniLandscapeSplineSegmentData& CachedSegment = CachedSegments[Idx];

			// Skip invalid segments
			if (!IsValid(Segment))
			{
				continue;
			}

			// Compare supported fields ...
			// TODO: could we simplify this by some kind of UProperty based diff, like UEdGraphNode does?
#if WITH_EDITORONLY_DATA
			if (Segment->LayerName != CachedSegment.LayerName
				|| Segment->bRaiseTerrain != CachedSegment.bRaiseTerrain
				|| Segment->bLowerTerrain != CachedSegment.bLowerTerrain)
			{
				return true;
			}

			const int32 NumSplineMeshes = Segment->SplineMeshes.Num(); 
			if (NumSplineMeshes != CachedSegment.SplineMeshes.Num())
			{
				return true;
			}

			// The FLandscapeSplineMeshEntry struct is not comparable with == operator
			for (int32 SplineMeshIdx = 0; SplineMeshIdx < NumSplineMeshes; ++SplineMeshIdx)
			{
				const FLandscapeSplineMeshEntry& SplineMeshEntry = Segment->SplineMeshes[SplineMeshIdx];
				const FLandscapeSplineMeshEntry& CachedSplineMeshEntry = CachedSegment.SplineMeshes[SplineMeshIdx];

				if (SplineMeshEntry.Mesh != CachedSplineMeshEntry.Mesh
						|| SplineMeshEntry.MaterialOverrides != CachedSplineMeshEntry.MaterialOverrides
						|| SplineMeshEntry.bCenterH != CachedSplineMeshEntry.bCenterH
						|| SplineMeshEntry.CenterAdjust != CachedSplineMeshEntry.CenterAdjust
						|| SplineMeshEntry.bScaleToWidth != CachedSplineMeshEntry.bScaleToWidth
						|| SplineMeshEntry.Scale != CachedSplineMeshEntry.Scale
						|| SplineMeshEntry.ForwardAxis != CachedSplineMeshEntry.ForwardAxis
						|| SplineMeshEntry.UpAxis != CachedSplineMeshEntry.UpAxis)
				{
					return true;
				}
			}
#endif
		}
	}

	return false;
}

//
UHoudiniInputHoudiniSplineComponent::UHoudiniInputHoudiniSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurveType(EHoudiniCurveType::Polygon)
	, CurveMethod(EHoudiniCurveMethod::CVs)
	, Reversed(false)
{
	bInputNodeHandleOverridesNodeIds = false;
}

//
UHoudiniInputHoudiniAsset::UHoudiniInputHoudiniAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetOutputIndex(-1)
	, AssetId(-1)
{
	bInputNodeHandleOverridesNodeIds = false;
}

//
UHoudiniInputActor::UHoudiniInputActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SplinesMeshObjectNodeId(-1)
	, SplinesMeshNodeId(-1)
	, LastUpdateNumComponentsAdded(0)
	, LastUpdateNumComponentsRemoved(0)
{

}

//
UHoudiniInputLevelInstance::UHoudiniInputLevelInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputLandscape::UHoudiniInputLandscape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedNumLandscapeComponents(0)
{

}

//
UHoudiniInputBrush::UHoudiniInputBrush()
	: CombinedModel(nullptr)
	, bIgnoreInputObject(false)
{

}

//
UHoudiniInputBlueprint::UHoudiniInputBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastUpdateNumComponentsAdded(0)
	, LastUpdateNumComponentsRemoved(0)
{

}

//
UHoudiniInputPackedLevelActor::UHoudiniInputPackedLevelActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BlueprintInputObject(nullptr)
{

}

//-----------------------------------------------------------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------------------------------------------------------

UObject* 
UHoudiniInputObject::GetObject() const
{
	return InputObject.LoadSynchronous();
}

void
UHoudiniInputObject::SetInputNodeId(const int32 InInputNodeId)
{
	// Only set the node id if the ref counted system is not used.
	if (bInputNodeHandleOverridesNodeIds)
		return;
	InputNodeId = InInputNodeId;
}

int32
UHoudiniInputObject::GetInputNodeId() const
{
	// If the ref counted system is enabled then we return the node id via the handle, otherwise return the id stored
	// on this object

	if (!bInputNodeHandleOverridesNodeIds)
		return InputNodeId;

	if (!InputNodeHandle.IsValid())
		return -1;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return -1;

	FUnrealObjectInputNode const* Node = nullptr;
	if (!Manager->GetNode(InputNodeHandle, Node))
		return -1;
	if (!Node)
		return -1;

	return Node->GetHAPINodeId();
}

void
UHoudiniInputObject::SetInputObjectNodeId(const int32 InInputObjectNodeId)
{
	// Only set the node id if the ref counted system is not used.
	if (bInputNodeHandleOverridesNodeIds)
		return;
	InputObjectNodeId = InInputObjectNodeId;
}

int32
UHoudiniInputObject::GetInputObjectNodeId() const
{
	// If the ref counted system is enabled then we return the node id via the handle, otherwise return the id stored
	// on this object

	if (!bInputNodeHandleOverridesNodeIds)
		return InputObjectNodeId;

	if (!InputNodeHandle.IsValid())
		return -1;

	const EUnrealObjectInputNodeType NodeType = InputNodeHandle.GetIdentifier().GetNodeType();
	if (NodeType != EUnrealObjectInputNodeType::Leaf && NodeType != EUnrealObjectInputNodeType::Reference)
		return -1;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return -1;

	FUnrealObjectInputNode const* Node = nullptr;
	if (!Manager->GetNode(InputNodeHandle, Node))
		return -1;
	if (!Node)
		return -1;

	FUnrealObjectInputLeafNode const* const LeafNode = static_cast<FUnrealObjectInputLeafNode const*>(Node);
	if (!LeafNode)
		return -1;

	return LeafNode->GetObjectHAPINodeId();
}

const TArray<FString>&
UHoudiniInputObject::GetMaterialReferences()
{
	UpdateMaterialReferences();
	return MaterialReferences;
}

UStaticMesh*
UHoudiniInputStaticMesh::GetStaticMesh() const
{
	return Cast<UStaticMesh>(InputObject.LoadSynchronous());
}

USkeletalMesh*
UHoudiniInputSkeletalMesh::GetSkeletalMesh()
{
	return Cast<USkeletalMesh>(InputObject.LoadSynchronous());
}

UAnimSequence*
UHoudiniInputAnimation::GetAnimation()
{
	return Cast<UAnimSequence>(InputObject.LoadSynchronous());
}

USkeletalMeshComponent*
UHoudiniInputSkeletalMeshComponent::GetSkeletalMeshComponent()
{
	return Cast<USkeletalMeshComponent>(InputObject.LoadSynchronous());
}

USkeletalMesh*
UHoudiniInputSkeletalMeshComponent::GetSkeletalMesh()
{
	USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent();
	if (!IsValid(SkeletalMeshComponent))
		return nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	return SkeletalMeshComponent->GetSkeletalMeshAsset();
#else
	return SkeletalMeshComponent->SkeletalMesh.Get();
#endif
}

UGeometryCollection*
UHoudiniInputGeometryCollection::GetGeometryCollection()
{
	return Cast<UGeometryCollection>(InputObject.LoadSynchronous());
}

UGeometryCollectionComponent*
UHoudiniInputGeometryCollectionComponent::GetGeometryCollectionComponent()
{
	return Cast<UGeometryCollectionComponent>(InputObject.LoadSynchronous());
}

UGeometryCollection* 
UHoudiniInputGeometryCollectionComponent::GetGeometryCollection()
{
	UGeometryCollectionComponent * GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (!IsValid(GeometryCollectionComponent))
	{
		return nullptr;
	}
	
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
	return GeometryCollectionEdit.GetRestCollection();
}

ALandscapeSplineActor*
UHoudiniInputLandscapeSplineActor::GetLandscapeSplineActor() const
{
	return Cast<ALandscapeSplineActor>(InputObject.LoadSynchronous());
}

ULandscapeSplinesComponent*
UHoudiniInputLandscapeSplinesComponent::GetLandscapeSplinesComponent() const
{
	return Cast<ULandscapeSplinesComponent>(InputObject.LoadSynchronous());
}

USceneComponent*
UHoudiniInputSceneComponent::GetSceneComponent()
{
	return Cast<USceneComponent>(InputObject.LoadSynchronous());
}

UStaticMeshComponent*
UHoudiniInputMeshComponent::GetStaticMeshComponent()
{
	return Cast<UStaticMeshComponent>(InputObject.LoadSynchronous());
}

UStaticMesh*
UHoudiniInputMeshComponent::GetStaticMesh() 
{ 
	return StaticMesh.Get(); 
}

UInstancedStaticMeshComponent*
UHoudiniInputInstancedMeshComponent::GetInstancedStaticMeshComponent() 
{
	return Cast<UInstancedStaticMeshComponent>(InputObject.LoadSynchronous());
}

USplineComponent*
UHoudiniInputSplineComponent::GetSplineComponent()
{
	return Cast<USplineComponent>(InputObject.LoadSynchronous());
}

UHoudiniSplineComponent*
UHoudiniInputHoudiniSplineComponent::GetCurveComponent() const
{
	return Cast<UHoudiniSplineComponent>(GetObject());
	//return Cast<UHoudiniSplineComponent>(InputObject.LoadSynchronous());
}

UCameraComponent*
UHoudiniInputCameraComponent::GetCameraComponent()
{
	return Cast<UCameraComponent>(InputObject.LoadSynchronous());
}

UHoudiniAssetComponent*
UHoudiniInputHoudiniAsset::GetHoudiniAssetComponent()
{
	return Cast<UHoudiniAssetComponent>(InputObject.LoadSynchronous());
}

AActor*
UHoudiniInputActor::GetActor() const
{
	AActor * Actor = Cast<AActor>(InputObject.LoadSynchronous());
	if (!Actor)
		return Actor;

	// Ensure transforms are up to date. It will not be if using sub levels.
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (IsValid(RootComponent))
		RootComponent->ConditionalUpdateComponentToWorld();

	return Actor;
}

ALevelInstance*
UHoudiniInputLevelInstance::GetLevelInstance() const
{
	return Cast<ALevelInstance>(InputObject.LoadSynchronous());
}

void
UHoudiniInputLevelInstance::InvalidateData()
{
	// Invalidate data on the tracked actor objects
	for (const auto& Entry : TrackedActorObjects)
	{
		UHoudiniInputObject* const TrackedObject = Entry.Value;
		if (!IsValid(TrackedObject))
			continue;
		TrackedObject->InvalidateData();
	}

	Super::InvalidateData();
}

ALandscapeProxy*
UHoudiniInputLandscape::GetLandscapeProxy() const
{
	return Cast<ALandscapeProxy>(InputObject.LoadSynchronous());
}

void 
UHoudiniInputLandscape::SetLandscapeProxy(UObject* InLandscapeProxy) 
{
	UObject* LandscapeProxy = Cast<UObject>(InLandscapeProxy);
	if (LandscapeProxy)
		InputObject = LandscapeProxy;
}

int32 UHoudiniInputLandscape::CountLandscapeComponents() const
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (!IsValid(LandscapeProxy))
	{
		return false;
	}
	
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!IsValid(LandscapeInfo))
	{
		return false; 
	}

	int32 NumComponents = 0;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	LandscapeInfo->ForEachLandscapeProxy([&NumComponents](ALandscapeProxy* Proxy)
#else
	LandscapeInfo->ForAllLandscapeProxies([&NumComponents](ALandscapeProxy* Proxy)
#endif
	{
		if (IsValid(Proxy))
		{
			NumComponents += Proxy->LandscapeComponents.Num();
		}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		return true;
#endif
	});
	return NumComponents;
}


ABrush*
UHoudiniInputBrush::GetBrush() const
{ 
	return Cast<ABrush>(InputObject.LoadSynchronous());
}


UBlueprint*
UHoudiniInputBlueprint::GetBlueprint() const
{
	return Cast<UBlueprint>(InputObject.LoadSynchronous());
}

//-----------------------------------------------------------------------------------------------------------------------------
// MARK CHANGED METHODS
//-----------------------------------------------------------------------------------------------------------------------------

void
UHoudiniInputObject::MarkChanged(const bool& bInChanged)
{
	bHasChanged = bInChanged;
	SetNeedsToTriggerUpdate(bInChanged);

	if (bInChanged && InputNodeHandle.IsValid())
	{
		static constexpr bool bAlsoDirtyReferencedNodes = true;
		FUnrealObjectInputRuntimeUtils::MarkInputNodeAsDirty(InputNodeHandle.GetIdentifier(), bAlsoDirtyReferencedNodes);
	}
}

void
UHoudiniInputActor::MarkChanged(const bool& bInChanged)
{
	bHasChanged = bInChanged;
	SetNeedsToTriggerUpdate(bInChanged);

	static constexpr bool bAlsoDirtyReferencedNodes = true;
	if (bInChanged && InputNodeHandle.IsValid())
		FUnrealObjectInputRuntimeUtils::MarkInputNodeAsDirty(InputNodeHandle.GetIdentifier(), bAlsoDirtyReferencedNodes);

	if (bInChanged && SplinesMeshInputNodeHandle.IsValid())
		FUnrealObjectInputRuntimeUtils::MarkInputNodeAsDirty(SplinesMeshInputNodeHandle.GetIdentifier(), bAlsoDirtyReferencedNodes);

	for (auto& CurComponent : ActorComponents)
	{
		CurComponent->MarkChanged(bInChanged);
	}
}

void
UHoudiniInputHoudiniSplineComponent::MarkChanged(const bool& bInChanged)
{
	Super::MarkChanged(bInChanged);

	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent)
	{
		HoudiniSplineComponent->MarkChanged(bInChanged);
	}
}


//-----------------------------------------------------------------------------------------------------------------------------
// CREATE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

UHoudiniInputObject *
UHoudiniInputObject::CreateTypedInputObject(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	if (!InObject)
		return nullptr;

	UHoudiniInputObject* HoudiniInputObject = nullptr;
	
	EHoudiniInputObjectType InputObjectType = GetInputObjectTypeFromObject(InObject);
	switch (InputObjectType)
	{
		case EHoudiniInputObjectType::Object:
			HoudiniInputObject = UHoudiniInputObject::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::StaticMesh:
			HoudiniInputObject = UHoudiniInputStaticMesh::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::SkeletalMesh:
			HoudiniInputObject = UHoudiniInputSkeletalMesh::Create(InObject, InOuter, InName, InInputSettings);
			break;
		case EHoudiniInputObjectType::Animation:
			HoudiniInputObject = UHoudiniInputAnimation::Create(InObject, InOuter, InName, InInputSettings);
			break;
		case EHoudiniInputObjectType::SceneComponent:
			// Do not create input objects for unknown scene component!
			//HoudiniInputObject = UHoudiniInputSceneComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::StaticMeshComponent:
			HoudiniInputObject = UHoudiniInputMeshComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
			HoudiniInputObject = UHoudiniInputInstancedMeshComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;
		case EHoudiniInputObjectType::SplineComponent:
			HoudiniInputObject = UHoudiniInputSplineComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::HoudiniSplineComponent:
			HoudiniInputObject = UHoudiniInputHoudiniSplineComponent::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::HoudiniAssetActor:
			{
				AHoudiniAssetActor* HoudiniActor = Cast<AHoudiniAssetActor>(InObject);
				if (HoudiniActor)
				{
					HoudiniInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniActor->GetHoudiniAssetComponent(), InOuter, InName, InInputSettings);
				}
				else
				{
					HoudiniInputObject = nullptr;
				}
			}
			break;

		case EHoudiniInputObjectType::HoudiniAssetComponent:
			HoudiniInputObject = UHoudiniInputHoudiniAsset::Create(InObject, InOuter, InName, InInputSettings);
			break;
		case EHoudiniInputObjectType::Actor:
		case EHoudiniInputObjectType::GeometryCollectionActor_Deprecated:
			HoudiniInputObject = UHoudiniInputActor::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::Landscape:
			HoudiniInputObject = UHoudiniInputLandscape::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::Brush:
			HoudiniInputObject = UHoudiniInputBrush::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::CameraComponent:
			HoudiniInputObject = UHoudiniInputCameraComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::DataTable:
			HoudiniInputObject = UHoudiniInputDataTable::Create(InObject, InOuter, InName, InInputSettings);
			break;
		
		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
			HoudiniInputObject = UHoudiniInputFoliageType_InstancedStaticMesh::Create(InObject, InOuter, InName, InInputSettings);
			break;
		case EHoudiniInputObjectType::GeometryCollection:
			HoudiniInputObject = UHoudiniInputGeometryCollection::Create(InObject, InOuter, InName, InInputSettings);
			break;
		case EHoudiniInputObjectType::GeometryCollectionComponent:
			HoudiniInputObject = UHoudiniInputGeometryCollectionComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::SkeletalMeshComponent:
			HoudiniInputObject = UHoudiniInputSkeletalMeshComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::Blueprint:
			HoudiniInputObject = UHoudiniInputBlueprint::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::LandscapeSplineActor:
			HoudiniInputObject = UHoudiniInputLandscapeSplineActor::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::LandscapeSplinesComponent:
			HoudiniInputObject = UHoudiniInputLandscapeSplinesComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::SplineMeshComponent:
			HoudiniInputObject = UHoudiniInputSplineMeshComponent::Create(InObject, InOuter, InName, InInputSettings, InParentInputActor);
			break;

		case EHoudiniInputObjectType::LevelInstance:
			HoudiniInputObject = UHoudiniInputLevelInstance::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::PackedLevelActor:
			HoudiniInputObject = UHoudiniInputPackedLevelActor::Create(InObject, InOuter, InName, InInputSettings);
			break;

		case EHoudiniInputObjectType::Invalid:
		default:
			break;
	}

	return HoudiniInputObject;
}


UHoudiniInputObject *
UHoudiniInputInstancedMeshComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_ISMC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputInstancedMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputInstancedMeshComponent * HoudiniInputObject = NewObject<UHoudiniInputInstancedMeshComponent>(
		InOuter, UHoudiniInputInstancedMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::InstancedStaticMeshComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputMeshComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_SMC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputMeshComponent * HoudiniInputObject = NewObject<UHoudiniInputMeshComponent>(
		InOuter, UHoudiniInputMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::StaticMeshComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputSplineComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_Spline_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSplineComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSplineComponent * HoudiniInputObject = NewObject<UHoudiniInputSplineComponent>(
		InOuter, UHoudiniInputSplineComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::SplineComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputHoudiniSplineComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_HoudiniSpline_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputHoudiniSplineComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputHoudiniSplineComponent * HoudiniInputObject = NewObject<UHoudiniInputHoudiniSplineComponent>(
		InOuter, UHoudiniInputHoudiniSplineComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::HoudiniSplineComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputCameraComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_Camera_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputCameraComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputCameraComponent * HoudiniInputObject = NewObject<UHoudiniInputCameraComponent>(
		InOuter, UHoudiniInputCameraComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::CameraComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputHoudiniAsset::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	UHoudiniAssetComponent * InHoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InObject);
	if (!InHoudiniAssetComponent)
		return nullptr;

	FString InputObjectNameStr = "HoudiniInputObject_HAC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputHoudiniAsset::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputHoudiniAsset * HoudiniInputObject = NewObject<UHoudiniInputHoudiniAsset>(
		InOuter, UHoudiniInputHoudiniAsset::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::HoudiniAssetComponent;

	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputSceneComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_SceneComp_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSceneComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSceneComponent * HoudiniInputObject = NewObject<UHoudiniInputSceneComponent>(
		InOuter, UHoudiniInputSceneComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::SceneComponent;
	HoudiniInputObject->ParentInputActor = InParentInputActor;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputLandscape::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_Landscape_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputLandscape::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputLandscape * HoudiniInputObject = NewObject<UHoudiniInputLandscape>(
		InOuter, UHoudiniInputLandscape::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Landscape;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject*
UHoudiniInputLevelInstance::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_LevelInstance_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputLevelInstance::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputLevelInstance* HoudiniInputObject = NewObject<UHoudiniInputLevelInstance>(
		InOuter, UHoudiniInputLevelInstance::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::LevelInstance;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;
	return HoudiniInputObject;
}


UHoudiniInputObject *
UHoudiniInputPackedLevelActor::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	const FString InputObjectNameStr = "HoudiniInputObject_PackedLevelActor_" + InName;
	const FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputPackedLevelActor::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputPackedLevelActor * HoudiniInputObject = NewObject<UHoudiniInputPackedLevelActor>(
		InOuter, UHoudiniInputPackedLevelActor::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::PackedLevelActor;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}


UHoudiniInputBrush *
UHoudiniInputBrush::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_Brush_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputBrush::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputBrush * HoudiniInputObject = NewObject<UHoudiniInputBrush>(
		InOuter, UHoudiniInputBrush::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Brush;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputActor::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_Actor_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputActor::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputActor * HoudiniInputObject = NewObject<UHoudiniInputActor>(
		InOuter, UHoudiniInputActor::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Actor;
	HoudiniInputObject->GeneratedSplinesMeshPackageGuid = FGuid::NewGuid();
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject*
UHoudiniInputBlueprint::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_BP_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputBlueprint::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputBlueprint* HoudiniInputObject = NewObject<UHoudiniInputBlueprint>(
		InOuter, UHoudiniInputBlueprint::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Blueprint;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputStaticMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_SM_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputStaticMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputStaticMesh * HoudiniInputObject = NewObject<UHoudiniInputStaticMesh>(
		InOuter, UHoudiniInputStaticMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::StaticMesh;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}


UHoudiniInputObject *
UHoudiniInputSkeletalMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_SkelMesh_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSkeletalMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSkeletalMesh * HoudiniInputObject = NewObject<UHoudiniInputSkeletalMesh>(
		InOuter, UHoudiniInputSkeletalMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::SkeletalMesh;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject*
UHoudiniInputAnimation::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_Animation_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputAnimation::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputAnimation* HoudiniInputObject = NewObject<UHoudiniInputAnimation>(
		InOuter, UHoudiniInputAnimation::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Animation;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}


UHoudiniInputObject*
UHoudiniInputSkeletalMeshComponent::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_SKC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSkeletalMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSkeletalMeshComponent* HoudiniInputObject = NewObject<UHoudiniInputSkeletalMeshComponent>(
		InOuter, UHoudiniInputSkeletalMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::SkeletalMeshComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputGeometryCollection::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_GC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputGeometryCollection::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputGeometryCollection * HoudiniInputObject = NewObject<UHoudiniInputGeometryCollection>(
                InOuter, UHoudiniInputGeometryCollection::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::GeometryCollection;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputGeometryCollectionComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	FString InputObjectNameStr = "HoudiniInputObject_GCC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputGeometryCollectionComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputGeometryCollectionComponent * HoudiniInputObject = NewObject<UHoudiniInputGeometryCollectionComponent>(
                InOuter, UHoudiniInputGeometryCollectionComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::GeometryCollectionComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}


UHoudiniInputObject*
UHoudiniInputLandscapeSplineActor::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	const FString InputObjectNameStr = "HoudiniInputObject_LandscapeSplines_" + InName;
	const FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputLandscapeSplineActor::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputLandscapeSplineActor* HoudiniInputObject = NewObject<UHoudiniInputLandscapeSplineActor>(
		InOuter, UHoudiniInputLandscapeSplineActor::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::LandscapeSplineActor;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}


UHoudiniInputObject*
UHoudiniInputLandscapeSplinesComponent::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	const FString InputObjectNameStr = "HoudiniInputObject_LandscapeSplines_" + InName;
	const FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputLandscapeSplinesComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputLandscapeSplinesComponent* HoudiniInputObject = NewObject<UHoudiniInputLandscapeSplinesComponent>(
		InOuter, UHoudiniInputLandscapeSplinesComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::LandscapeSplinesComponent;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;
	// Ensure that NextControlPointId is valid
	if (HoudiniInputObject->NextControlPointId < 0)
		HoudiniInputObject->NextControlPointId = 0;

	return HoudiniInputObject;
}


UHoudiniInputObject *
UHoudiniInputObject::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputObject::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputObject * HoudiniInputObject = NewObject<UHoudiniInputObject>(
		InOuter, UHoudiniInputObject::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Object;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

bool
UHoudiniInputObject::Matches(const UHoudiniInputObject& Other) const
{
	bool Matches = (Type == Other.Type 
		&& InputNodeId == Other.InputNodeId
		&& InputObjectNodeId == Other.InputObjectNodeId
		);

	Matches &= InputNodeHandle == Other.InputNodeHandle;

	return Matches;
}

//-----------------------------------------------------------------------------------------------------------------------------
// DELETE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

void
UHoudiniInputObject::InvalidateData()
{
	// If valid, mark our input nodes for deletion..	
	if (this->IsA<UHoudiniInputHoudiniAsset>() || !CanDeleteHoudiniNodes())
	{
		// Unless if we're a HoudiniAssetInput! we don't want to delete the other HDA's node!
		// just invalidate the node IDs!
		InputNodeId = -1;
		InputObjectNodeId = -1;
		InputNodeHandle.Reset();
		return;
	}

	// If we are using the new input system, then don't delete the nodes if we have a valid handle, and the HAPI
	// nodes associated with the handle matches InputNodeId / InputObjectNodeId
	if (InputNodeHandle.IsValid())
	{
		IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
		if (Manager)
		{
			TArray<int32> ManagedNodeIds;
			if (Manager->GetHAPINodeIds(InputNodeHandle.GetIdentifier(), ManagedNodeIds))
			{
				if (ManagedNodeIds.Contains(InputNodeId))
					InputNodeId = -1;
				if (ManagedNodeIds.Contains(InputObjectNodeId))
					InputObjectNodeId = -1;
			}
		}
	}

	InputNodeHandle.Reset();
	
	if (InputNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputNodeId);
		InputNodeId = -1;
	}

	// ... and the parent OBJ as well to clean up
	if (InputObjectNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputObjectNodeId);
		InputObjectNodeId = -1;
	}

	
}

void
UHoudiniInputObject::BeginDestroy()
{
	// Invalidate and mark our input node for deletion
	InvalidateData();

	Super::BeginDestroy();
}

//-----------------------------------------------------------------------------------------------------------------------------
// UPDATE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

void
UHoudiniInputObject::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	InputObject = InObject;
	CachedInputSettings = InSettings;
}

FString UHoudiniInputObject::FormatAssetReference(FString AssetReference) {
	// Replace the first space with a single quote
	for (int32 Itr = 0; Itr < AssetReference.Len(); Itr++)
	{
		if (AssetReference[Itr] == ' ')
		{
			AssetReference[Itr] = '\'';
			break;
		}
	}

	// Attach another single quote to the end
	AssetReference += FString("'");
	return AssetReference;
}

void
UHoudiniInputObject::UpdateMaterialReferences()
{
	UObject* InObject = GetObject();
	if (!InObject)
		return;

	MaterialReferences.Empty();

	EHoudiniInputObjectType InputObjectType = GetInputObjectTypeFromObject(InObject);
	switch (InputObjectType)
	{
		case EHoudiniInputObjectType::StaticMesh:
		{
			const UStaticMesh* SM = Cast<UStaticMesh>(InObject);
			ensure(SM);

			const TArray<FStaticMaterial> Materials = SM->GetStaticMaterials();
			for (const FStaticMaterial& Material : Materials)
			{
				FString AssetReference = FormatAssetReference(Material.MaterialInterface->GetFullName());
				MaterialReferences.Add(AssetReference);
			}
			break;
		}

		case EHoudiniInputObjectType::SkeletalMesh:
		{
			const USkeletalMesh* SK = Cast<USkeletalMesh>(InObject);
			ensure(SK);

			const TArray<FSkeletalMaterial> Materials = SK->GetMaterials();
			for (const FSkeletalMaterial& Material : Materials)
			{
				FString AssetReference = FormatAssetReference(Material.MaterialInterface->GetFullName());
				MaterialReferences.Add(AssetReference);
			}
			break;
		}

		case EHoudiniInputObjectType::StaticMeshComponent:
		case EHoudiniInputObjectType::SkeletalMeshComponent:
		case EHoudiniInputObjectType::GeometryCollectionComponent:
		{
			const UMeshComponent* MC = Cast<UMeshComponent>(InObject);
			ensure(MC);

			const TArray<UMaterialInterface*> Materials = MC->GetMaterials();
			for (const UObject* Material : Materials)
			{
				FString AssetReference = FormatAssetReference(Material->GetFullName());
				MaterialReferences.Add(AssetReference);
			}

			break;
		}

		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
		{
			const UFoliageType_InstancedStaticMesh* FT = Cast<UFoliageType_InstancedStaticMesh>(InObject);
			ensure(FT);
			const UStaticMesh* SM = FT->GetStaticMesh();

			// Use the override materials from the Instancer if available, otherwise use the original materials from the instanced Static Mesh
			const TArray<UMaterialInterface*> OverrideMaterials = FT->OverrideMaterials;
			const TArray<FStaticMaterial> StaticMaterials = SM->GetStaticMaterials();
			for (const UObject* Material : OverrideMaterials)
			{
				FString AssetReference = FormatAssetReference(Material->GetFullName());
				MaterialReferences.Add(AssetReference);
			}
			
			if (OverrideMaterials.Num() > 0)
				break;

			for (const FStaticMaterial& Material : StaticMaterials)
			{
				FString AssetReference = FormatAssetReference(Material.MaterialInterface->GetFullName());
				MaterialReferences.Add(AssetReference);
			}

			break;
		}

		case EHoudiniInputObjectType::GeometryCollection:
		{
			UGeometryCollection* const GC = Cast<UGeometryCollection>(InObject);
			ensure(GC);

			const TArray<UMaterialInterface*> Materials = GC->Materials;

			for (const UObject* Material : Materials)
			{
				FString AssetReference = FormatAssetReference(Material->GetFullName());
				MaterialReferences.Add(AssetReference);
			}
			break;
		}

		default:
			break;
	}
}

void
UHoudiniInputStaticMesh::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);
	// Static Mesh input accepts SM, BP, FoliageType_InstancedStaticMesh (static mesh) and FoliageType_Actor (if blueprint actor).
	UStaticMesh* SM = Cast<UStaticMesh>(InObject);
	UBlueprint* BP = Cast<UBlueprint>(InObject);

	ensure(SM || BP);
}

void
UHoudiniInputSkeletalMesh::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);

	USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(InObject);
	ensure(SkelMesh);
}

void
UHoudiniInputAnimation::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);

	UAnimSequence* Animation = Cast<UAnimSequence>(InObject);
	ensure(Animation);
}


void
UHoudiniInputSkeletalMeshComponent::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObject);
	ensure(SkeletalMeshComponent);
}


void
UHoudiniInputGeometryCollection::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);

	UGeometryCollection* GeometryCollection = Cast<UGeometryCollection>(InObject);
	ensure(GeometryCollection);
}


void
UHoudiniInputGeometryCollectionComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Nothing to do
	Super::Update(InObject, InSettings);

	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(InObject);
	ensure(GeometryCollectionComponent);
}

void
UHoudiniInputLandscapeSplineActor::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Get the current value for if exporting spline meshes is enabled
	if (!InSettings.bLandscapeSplinesExportSplineMeshComponents || !InSettings.bMergeSplineMeshComponents)
	{
		// Invalidate the merged splines nodes
		InvalidateSplinesMeshData();
	}

	Super::Update(InObject, InSettings);
}

void
UHoudiniInputLandscapeSplinesComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	ULandscapeSplinesComponent* const LandscapeSplinesComponent = Cast<ULandscapeSplinesComponent>(InObject);
	ensure(LandscapeSplinesComponent);

	// Cache the control points and segments from landscape splines component, for use in HasComponentChanged()
	// Use helper to fetch control points since the landscape splines API differs between UE 5.0 and 5.1+
	TArray<TObjectPtr<ULandscapeSplineControlPoint>> ControlPoints;
	TArray<TObjectPtr<ULandscapeSplineSegment>> Segments;
	FHoudiniEngineRuntimeUtils::GetLandscapeSplinesControlPointsAndSegments(LandscapeSplinesComponent, &ControlPoints, &Segments);
	
	const int32 NumControlPoints = ControlPoints.Num();

	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32> OldControlPointIdMap(ControlPointIdMap);
	ControlPointIdMap.Empty(NumControlPoints);

	CachedControlPoints.SetNum(NumControlPoints);
	if (NumControlPoints > 0)
	{
		for (int32 Idx = 0; Idx < NumControlPoints; ++Idx)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			TObjectPtr<ULandscapeSplineControlPoint>& ControlPoint = ControlPoints[Idx];
#else
			ULandscapeSplineControlPoint const* const ControlPoint = ControlPoints[Idx];
#endif
			FHoudiniLandscapeSplineControlPointData& CachedControlPoint = CachedControlPoints[Idx];

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			if (!ControlPoint)
#else
			if (!IsValid(ControlPoint))
#endif
			{
				// Reset entry to default
				CachedControlPoint = {};
				continue;
			}

			// See if we already have a valid id for this control point and copy it
			bool bHasValidControlPointId = false;
			TSoftObjectPtr<ULandscapeSplineControlPoint> ControlPointPtr(ControlPoint);
			if (int32 const* const ControlPointIdPtr = OldControlPointIdMap.Find(ControlPointPtr))
			{
				const int32 ControlPointId = *ControlPointIdPtr;
				if (ControlPointId >= 0)
				{
					ControlPointIdMap.Add(ControlPointPtr, ControlPointId);
					bHasValidControlPointId = true;
				}
			}
			if (!bHasValidControlPointId)
			{
				// Generate a new valid id for ControlPoint and update the map
				FHoudiniLandscapeRuntimeUtils::GetOrGenerateValidControlPointId(ControlPoint, ControlPointIdMap, NextControlPointId);
			}

			CachedControlPoint.Location = ControlPoint->Location;
			CachedControlPoint.Rotation = ControlPoint->Rotation;
			CachedControlPoint.Width = ControlPoint->Width;
#if WITH_EDITORONLY_DATA
			CachedControlPoint.SegmentMeshOffset = ControlPoint->SegmentMeshOffset;
			CachedControlPoint.LayerName = ControlPoint->LayerName;
			CachedControlPoint.bRaiseTerrain = ControlPoint->bRaiseTerrain;
			CachedControlPoint.bLowerTerrain = ControlPoint->bLowerTerrain;
			CachedControlPoint.Mesh = ControlPoint->Mesh;
			CachedControlPoint.MaterialOverrides = ControlPoint->MaterialOverrides;
			CachedControlPoint.MeshScale = ControlPoint->MeshScale;
#endif
		}
	}

	const int32 NumSegments = Segments.Num();
	CachedSegments.SetNum(NumSegments);
	if (NumSegments > 0)
	{
		for (int32 Idx = 0; Idx < NumSegments; ++Idx)
		{
			ULandscapeSplineSegment const* const Segment = Segments[Idx];
			FHoudiniLandscapeSplineSegmentData& CachedSegment = CachedSegments[Idx];
			if (!IsValid(Segment))
			{
				// Reset entry to default
				CachedSegment = {};
				continue;
			}

#if WITH_EDITORONLY_DATA
			CachedSegment.LayerName = Segment->LayerName;
			CachedSegment.bRaiseTerrain = Segment->bRaiseTerrain;
			CachedSegment.bLowerTerrain = Segment->bLowerTerrain;
			CachedSegment.SplineMeshes = Segment->SplineMeshes;
#endif
		}
	}
}


void
UHoudiniInputSceneComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{	
	Super::Update(InObject, InSettings);
	UpdateTransform();
}

void
UHoudiniInputSceneComponent::UpdateTransform()
{
	USceneComponent* const USC = GetSceneComponent();
	ensure(USC);
	if (IsValid(USC))
	{
		// For components in Blueprints we have to ensure that the ComponentToWorld is calculated
		USC->ConditionalUpdateComponentToWorld();
		Transform = USC->GetComponentTransform();
	}
	
	if (IsValid(ParentInputActor))
		ActorTransform = ParentInputActor->GetHoudiniObjectTransform();
	else
		ActorTransform = FTransform::Identity;
}

bool 
UHoudiniInputSceneComponent::HasActorTransformChanged() const
{
	// Returns true if the attached actor's (parent) transform has been modified
	if (!IsValid(ParentInputActor))
		return false;

	UHoudiniInputActor const* const InputActor = Cast<UHoudiniInputActor>(ParentInputActor);
	if (IsValid(InputActor) && InputActor->HasActorTransformChanged())
		return true;

	return !ActorTransform.Equals(ParentInputActor->GetHoudiniObjectTransform());
}


bool
UHoudiniInputSceneComponent::HasComponentTransformChanged() const
{
	// Returns true if the attached actor's (parent) transform has been modified
	USceneComponent* MyComp = Cast<USceneComponent>(InputObject.LoadSynchronous());
	if (!IsValid(MyComp))
		return false;

	// For components in Blueprints we have to ensure that the ComponentToWorld is calculated
	MyComp->ConditionalUpdateComponentToWorld();
	return !Transform.Equals(MyComp->GetComponentTransform());
}


bool 
UHoudiniInputSceneComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	// Should return true if the component itself has been modified
	// Should be overriden in child classes
	return false;
}

FTransform
UHoudiniInputSceneComponent::GetHoudiniObjectTransform() const
{
	return GetTransformRelativeToOwner();
}

bool
UHoudiniInputMeshComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InputObject.LoadSynchronous());
	UStaticMesh* SMCSM = nullptr;
	if (IsValid(SMC))
		SMCSM = SMC->GetStaticMesh();
	
	UStaticMesh* MySM = StaticMesh.Get();

	// Return true if SMC's static mesh has been modified
	return (MySM != SMCSM);
}

bool
UHoudiniInputCameraComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	UCameraComponent* Camera = Cast<UCameraComponent>(InputObject.LoadSynchronous());	
	if (IsValid(Camera))
	{
		bool bOrtho = Camera->ProjectionMode == ECameraProjectionMode::Type::Orthographic;
		if (bOrtho != bIsOrthographic)
			return true;

		if (Camera->FieldOfView != FOV)
			return true;

		if (Camera->AspectRatio != AspectRatio)
			return true;

		if (Camera->OrthoWidth != OrthoWidth)
			return true;

		if (Camera->OrthoNearClipPlane != OrthoNearClipPlane)
			return true;

		if (Camera->OrthoFarClipPlane != OrthoFarClipPlane)
			return true;
	}
		
	return false;
}



void
UHoudiniInputCameraComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	UCameraComponent* Camera = Cast<UCameraComponent>(InputObject.LoadSynchronous());

	ensure(Camera);
	
	if (IsValid(Camera))
	{	
		bIsOrthographic = Camera->ProjectionMode == ECameraProjectionMode::Type::Orthographic;
		FOV = Camera->FieldOfView;
		AspectRatio = Camera->AspectRatio;
		OrthoWidth = Camera->OrthoWidth;
		OrthoNearClipPlane = Camera->OrthoNearClipPlane;
		OrthoFarClipPlane = Camera->OrthoFarClipPlane;
	}
}

void
UHoudiniInputMeshComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InObject);

	ensure(SMC);

	if (IsValid(SMC))
	{
		StaticMesh = TSoftObjectPtr<UStaticMesh>(SMC->GetStaticMesh());
	}
}

void
UHoudiniInputInstancedMeshComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InObject);

	ensure(ISMC);

	if (ISMC)
	{
		uint32 InstanceCount = ISMC->GetInstanceCount();
		InstanceTransforms.SetNum(InstanceCount);

		// Copy the instances' transforms		
		for (uint32 InstIdx = 0; InstIdx < InstanceCount; InstIdx++)
		{
			FTransform CurTransform = FTransform::Identity;
			ISMC->GetInstanceTransform(InstIdx, CurTransform);
			InstanceTransforms[InstIdx] = CurTransform;			
		}
	}
}

bool
UHoudiniInputInstancedMeshComponent::HasInstancesChanged() const
{	
	UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InputObject.LoadSynchronous());
	if (!ISMC)
		return false;

	uint32 InstanceCount = ISMC->GetInstanceCount();
	if (InstanceTransforms.Num() != InstanceCount)
		return true;

	// Copy the instances' transforms		
	for (uint32 InstIdx = 0; InstIdx < InstanceCount; InstIdx++)
	{
		FTransform CurTransform = FTransform::Identity;
		ISMC->GetInstanceTransform(InstIdx, CurTransform);

		if(!InstanceTransforms[InstIdx].Equals(CurTransform))
			return true;
	}

	return false;
}

bool
UHoudiniInputInstancedMeshComponent::HasComponentTransformChanged() const
{
	if (Super::HasComponentTransformChanged())
		return true;

	return HasInstancesChanged();
}

void
UHoudiniInputSplineComponent::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	USplineComponent* Spline = Cast<USplineComponent>(InObject);

	ensure(Spline);

	if (Spline)
	{
		NumberOfSplineControlPoints = Spline->GetNumberOfSplinePoints();
		SplineLength = Spline->GetSplineLength();
		SplineClosed = Spline->IsClosedLoop();

		//SplineResolution = -1.0f;

		SplineControlPoints.SetNumZeroed(NumberOfSplineControlPoints);
		for (int32 Idx = 0; Idx < NumberOfSplineControlPoints; Idx++)
		{
			SplineControlPoints[Idx] = Spline->GetTransformAtSplinePoint(Idx, ESplineCoordinateSpace::Local, true);
		}		
	}
}

void
UHoudiniInputHoudiniSplineComponent::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	// We store the component references as a normal pointer property instead of using a soft object reference.
	// If we use a soft object reference, the editor will complain about deleting a reference that is in use 
	// everytime we try to delete the actor, even though everything is contained within the actor.

	CachedComponent = Cast<UHoudiniSplineComponent>(InObject);
	InputObject = nullptr;

	// We need a strong ref to the spline component to prevent it from being GCed
	//MyHoudiniSplineComponent = Cast<UHoudiniSplineComponent>(InObject);
	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();

	if (!IsValid(HoudiniSplineComponent))
	{
		// Use default values
		CurveType = EHoudiniCurveType::Polygon;
		CurveMethod = EHoudiniCurveMethod::CVs;
		Reversed = false;
	}
	else
	{
		CurveType = HoudiniSplineComponent->GetCurveType();
		CurveMethod = HoudiniSplineComponent->GetCurveMethod();
		Reversed = false;//Spline->IsReversed();
	}
}

UObject*
UHoudiniInputHoudiniSplineComponent::GetObject() const
{
	return CachedComponent;
}

void
UHoudiniInputHoudiniSplineComponent::SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate)
{
	Super::SetNeedsToTriggerUpdate(bInTriggersUpdate);

	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent)
	{
		HoudiniSplineComponent->SetNeedsToTriggerUpdate(bInTriggersUpdate);
	}
}

bool
UHoudiniInputHoudiniSplineComponent::HasChanged() const
{
	if (Super::HasChanged())
		return true;

	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent && HoudiniSplineComponent->HasChanged())
		return true;

	return false;
}

bool
UHoudiniInputHoudiniSplineComponent::NeedsToTriggerUpdate() const
{
	if (Super::NeedsToTriggerUpdate())
		return true;
	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent && HoudiniSplineComponent->NeedsToTriggerUpdate())
		return true;

	return false;
}

void
UHoudiniInputHoudiniAsset::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InObject);

	ensure(HAC);

	if (HAC)
	{
		// TODO: Allow selection of the asset output
		AssetOutputIndex = 0;
		AssetId = HAC->GetAssetId();
	}
}


void
UHoudiniInputActor::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	const bool bHasInputObjectChanged = InputObject != InObject;
	
	Super::Update(InObject, InSettings);

	// Ensure we have a valid GUID for merged spline mesh components
	if (!GeneratedSplinesMeshPackageGuid.IsValid())
		GeneratedSplinesMeshPackageGuid = FGuid::NewGuid();

	AActor* Actor = Cast<AActor>(InObject);
	ensure(Actor);

	if (Actor)
	{
		Transform = Actor->GetTransform();

		// If we are updating (InObject == InputObject), then remove stale components and add new components,
		// if InObject != InputObject, remove all components and rebuild

		if (bHasInputObjectChanged)
		{
			// The actor's components that can be sent as inputs
			LastUpdateNumComponentsRemoved = ActorComponents.Num();

			ActorComponents.Empty();
			ActorSceneComponents.Empty();
			NumSplineMeshComponents = 0;

			TArray<USceneComponent*> AllComponents;
			Actor->GetComponents<USceneComponent>(AllComponents, true);
			
			ActorComponents.Reserve(AllComponents.Num());
			for (USceneComponent * SceneComponent : AllComponents)
			{
				if (!IsValid(SceneComponent))
					continue;
				if (!ShouldTrackComponent(SceneComponent))
					continue;

				UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
					SceneComponent, GetOuter(), Actor->GetActorNameOrLabel(), InSettings, this);
				if (!InputObj)
					continue;

				UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
				if (!SceneInput)
					continue;

				if (SceneInput->IsA<UHoudiniInputSplineMeshComponent>())
					NumSplineMeshComponents++;

				ActorComponents.Add(SceneInput);
				ActorSceneComponents.Add(TSoftObjectPtr<UObject>(SceneComponent));
			}
			LastUpdateNumComponentsAdded = ActorComponents.Num();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;
			NumSplineMeshComponents = 0;

			// Look for any components to add or remove
			TSet<USceneComponent*> NewComponents;
			const bool bIncludeFromChildActors = true;
			Actor->ForEachComponent<USceneComponent>(bIncludeFromChildActors, [&](USceneComponent* InComp)
			{
				if (!IsValid(InComp))
					return;
				if (!ShouldTrackComponent(InComp))
					return;
				
				if (InComp->IsA<USplineMeshComponent>())
					NumSplineMeshComponents++;
				
				if (ActorSceneComponents.Contains(InComp))
					return;
				
				NewComponents.Add(InComp);
			});
			
			// Update the actor input components (from the same actor)
			TArray<int32> ComponentIndicesToRemove;
			const int32 NumActorComponents = ActorComponents.Num(); 
			for (int32 Index = 0; Index < NumActorComponents; ++Index)
			{
				UHoudiniInputSceneComponent* CurActorComp = ActorComponents[Index];
				if (!IsValid(CurActorComp))
				{
					ComponentIndicesToRemove.Add(Index);
					continue;
				}

				// If the tracked object is no longer valid, remove the input component.
				// If the tracked object is valid, and it is a scene component, then if we should no longer be tracking
				// components of that type remove it.
				bool bShouldRemove = !IsValid(CurActorComp->GetObject());
				if (!bShouldRemove)
				{
					USceneComponent* const InputComp = CurActorComp->GetSceneComponent();
					if (IsValid(InputComp) && !ShouldTrackComponent(InputComp))
						bShouldRemove = true;
				}
				if (bShouldRemove)
				{
					// If it's not, mark it for deletion
					if ((CurActorComp->GetInputNodeId() > 0) || (CurActorComp->GetInputObjectNodeId() > 0))
					{
						CurActorComp->InvalidateData();
					}

					ComponentIndicesToRemove.Add(Index);
					continue;
				}

				// If the component is valid and should still be tracked, but it doesn't have the new ParentInputActor
				// property set, then it needs to be recreated
				if (!CurActorComp->GetParentInputActor())
				{
					ComponentIndicesToRemove.Add(Index);
					NewComponents.Add(CurActorComp->GetSceneComponent());
				}
			}

			// Remove the destroyed/invalid components
			const int32 NumToRemove = ComponentIndicesToRemove.Num();
			if (NumToRemove > 0)
			{
				for (int32 Index = NumToRemove - 1; Index >= 0; --Index)
				{
					const int32& IndexToRemove = ComponentIndicesToRemove[Index];
					
					UHoudiniInputSceneComponent* const CurActorComp = ActorComponents[IndexToRemove];
					if (CurActorComp)
					{
						ActorSceneComponents.Remove(CurActorComp->InputObject);
					}

					const bool bAllowShrink = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
					ActorComponents.RemoveAtSwap(IndexToRemove, 1, bAllowShrink ? EAllowShrinking::Yes : EAllowShrinking::No);
#else
					ActorComponents.RemoveAtSwap(IndexToRemove, 1, bAllowShrink);
#endif

					LastUpdateNumComponentsRemoved++;
				}
			}

			if (NewComponents.Num() > 0)
			{
				for (USceneComponent * SceneComponent : NewComponents)
				{
					if (!IsValid(SceneComponent))
						continue;

					UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
						SceneComponent, GetOuter(), Actor->GetActorNameOrLabel(), InSettings, this);
					if (!InputObj)
						continue;

					UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
					if (!SceneInput)
						continue;

					ActorComponents.Add(SceneInput);
					ActorSceneComponents.Add(SceneComponent);

					LastUpdateNumComponentsAdded++;
				}
			}

			if (LastUpdateNumComponentsAdded > 0 || LastUpdateNumComponentsRemoved > 0)
			{
				ActorComponents.Shrink();
			}
		}
	}
	else
	{
		// If we don't have a valid actor or null, delete any input components we still have and mark as changed
		if (ActorComponents.Num() > 0)
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = ActorComponents.Num();
			ActorComponents.Empty();
			ActorSceneComponents.Empty();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;
		}
		NumSplineMeshComponents = 0;
	}
}

bool 
UHoudiniInputActor::HasActorTransformChanged() const
{
	if (!GetActor())
		return false;

	if (HasRootComponentTransformChanged())
		return true;

	if (HasComponentsTransformChanged())
		return true;

	return false;
}

bool UHoudiniInputActor::HasRootComponentTransformChanged() const
{
	if (!Transform.Equals(GetActor()->GetTransform()))
		return true;
	return false;
}

bool UHoudiniInputActor::HasComponentsTransformChanged() const
{
	// Search through all the child components to see if we have changed
	// transforms in there.
	for (auto CurActorComp : GetActorComponents())
	{
		if (CurActorComp->HasComponentTransformChanged())
		{
			return true;
		}
	}

	return false;
}

bool 
UHoudiniInputActor::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	// In the new input system, treat the input actor as changed if it has any component changes
	{
		if (GetLastUpdateNumComponentsAdded() > 0 || GetLastUpdateNumComponentsRemoved() > 0)
			return true;
		for (UHoudiniInputSceneComponent const* const Component : ActorComponents)
		{
			if (!IsValid(Component))
				continue;
			if (Component->HasComponentChanged(InSettings))
				return true;
		}
	}
	
	// Check if any of the spline mesh components that we are tracking has changed
	TSet<UActorComponent const*> TrackedSMComponents; 
	bool bSplineMeshComponentChanged = false;
	for (UHoudiniInputSceneComponent const* const Component : ActorComponents)
	{
		if (!IsValid(Component))
			continue;

		UHoudiniInputSplineMeshComponent const* const SMComponent = Cast<UHoudiniInputSplineMeshComponent>(Component);
		if (!IsValid(SMComponent))
			continue;
		
		TrackedSMComponents.Add(SMComponent->GetSplineMeshComponent());
		if (!SMComponent->HasComponentTransformChanged() && !SMComponent->HasComponentChanged(InSettings))
			continue;
		
		bSplineMeshComponentChanged = true;
	}

	// Count the number of spline mesh components the actor has
	AActor const* const Actor = GetActor();
	int32 CurrentNumSplineMeshComponents = 0;
	if (IsValid(Actor))
	{
		static constexpr bool bIncludeFromChildActors = false;
		Actor->ForEachComponent<USplineMeshComponent>(bIncludeFromChildActors, [this, &InSettings, &CurrentNumSplineMeshComponents, &bSplineMeshComponentChanged, &TrackedSMComponents](UActorComponent const* const InComponent)
		{
			if (!this->ShouldTrackComponent(InComponent, &InSettings))
				return;
			
			CurrentNumSplineMeshComponents++;

			if (!TrackedSMComponents.Contains(InComponent))
				bSplineMeshComponentChanged = true;
		});
	}

	// This actor should be considered changed if:
	//		- Merge is switching from enabled to disabled, or disabled to enabled
	//		- Merging is enabled and at least one spline mesh component has changed, or the number of spline mesh components have changed
	const bool bNeedsToMergeSplineMeshes = InSettings.bMergeSplineMeshComponents && CurrentNumSplineMeshComponents > 1;
	if (bUsedMergeSplinesMeshAtLastTranslate != bNeedsToMergeSplineMeshes)
		return true;

	if (bNeedsToMergeSplineMeshes && (bSplineMeshComponentChanged || NumSplineMeshComponents != CurrentNumSplineMeshComponents))
		return true;

	return false;
}

bool
UHoudiniInputActor::GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects)
{
	if (Super::GetChangedObjectsAndValidNodes(OutChangedObjects, OutNodeIdsOfUnchangedValidObjects))
		return true;

	// If the actor is merging spline mesh components then it should have a valid SplinesMeshObjectNodeId 
	if (bUsedMergeSplinesMeshAtLastTranslate)
	{
		if (SplinesMeshObjectNodeId >= 0)
		{
			// Old input system code removed.
		}
		else
		{
			OutChangedObjects.Add(this);
			return true;
		}
	}
	
	bool bAnyChanges = false;

	return bAnyChanges;
}

void
UHoudiniInputActor::InvalidateSplinesMeshData()
{
	GeneratedSplinesMesh = nullptr;
	// If valid, mark our input nodes for deletion.
	if (!CanDeleteHoudiniNodes())
	{
		// Unless if we're a HoudiniAssetInput! we don't want to delete the other HDA's node!
		// just invalidate the node IDs!
		SplinesMeshNodeId = -1;
		SplinesMeshObjectNodeId = -1;
		SplinesMeshInputNodeHandle.Reset();
	}
	else
	{
		// If we are using the new input system, then don't delete the nodes if we have a valid handle, and the HAPI
		// nodes associated with the handle matches SplinesMeshNodeId / SplinesMeshObjectNodeId
		if (SplinesMeshInputNodeHandle.IsValid())
		{
			IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
			if (Manager)
			{
				TArray<int32> ManagedNodeIds;
				if (Manager->GetHAPINodeIds(SplinesMeshInputNodeHandle.GetIdentifier(), ManagedNodeIds))
				{
					if (ManagedNodeIds.Contains(SplinesMeshNodeId))
						SplinesMeshNodeId = -1;
					if (ManagedNodeIds.Contains(SplinesMeshObjectNodeId))
						SplinesMeshObjectNodeId = -1;
				}
			}
		}

		SplinesMeshInputNodeHandle.Reset();
		
		if (SplinesMeshNodeId >= 0)
		{
			FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(SplinesMeshNodeId);
			SplinesMeshNodeId = -1;
		}

		// ... and the parent OBJ as well to clean up
		if (SplinesMeshObjectNodeId >= 0)
		{
			FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(SplinesMeshObjectNodeId);
			SplinesMeshObjectNodeId = -1;
		}
	}
}

bool
UHoudiniInputActor::UsesInputObjectNode() const
{
	return true;
}

void
UHoudiniInputActor::InvalidateData()
{
	InvalidateSplinesMeshData();
	
	// Call invalidate on input component objects
	for (auto* const CurrentComp : GetActorComponents())
	{
		if (!IsValid(CurrentComp))
			continue;
		
		CurrentComp->InvalidateData();
	}
	
	Super::InvalidateData();
}

bool
UHoudiniInputActor::ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const
{
	return true;
}

void UHoudiniInputLevelInstance::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	NumActorsAddedLastUpdate = 0;
	NumActorsRemovedLastUpdate = 0;
	
	if (!CachedInputSettings.bExportLevelInstanceContent)
	{
		NumActorsRemovedLastUpdate = TrackedActorObjects.Num();
		TrackedActorObjects.Empty();
		return;
	}

	// We are exporting level content, make sure our TrackedActorObjects set is up to date
	ALevelInstance* const LevelInstance = GetLevelInstance();
	if (!IsValid(LevelInstance))
	{
		NumActorsRemovedLastUpdate = TrackedActorObjects.Num();
		TrackedActorObjects.Empty();
		return;
	}

	UWorld* World = LevelInstance->GetWorldAsset().LoadSynchronous();
	if (!IsValid(World))
	{
		NumActorsRemovedLastUpdate = TrackedActorObjects.Num();
		TrackedActorObjects.Empty();
		return;
	}

	// Identify current and new actors
	TSet<AActor*> CurrentActors;
	CurrentActors.Reserve(World->GetActorCount());
	TSet<AActor*> NewActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* const Actor = *It;
		if (!IsValid(Actor))
			continue;

		CurrentActors.Emplace(Actor);
		if (TrackedActorObjects.Contains(Actor))
			continue;
		NewActors.Add(Actor);
	}

	// Remove actors from tracked set that are no longer in level instance
	TArray<TSoftObjectPtr<AActor>> Keys;
	TrackedActorObjects.GetKeys(Keys);
	for (TSoftObjectPtr<AActor>& Actor : Keys)
	{
		AActor const* const LoadedActor = Actor.LoadSynchronous();
		if (IsValid(LoadedActor) && CurrentActors.Contains(LoadedActor))
			continue;

		TrackedActorObjects.Remove(Actor);
		NumActorsRemovedLastUpdate++;
	}

	// Call Update on all tracked input objects (except for the new actors in NewActors: Update will be called in CreateTypeInputObject below)
	for (const auto& Entry : TrackedActorObjects)
	{
		UHoudiniInputObject* const TrackedInputObject = Entry.Value;
		if (!IsValid(TrackedInputObject))
			continue;
		TrackedInputObject->Update(Entry.Key.Get(), InSettings);
	}

	// Add / track new actors
	for (AActor* const NewActor : NewActors)
	{
		if (!IsValid(NewActor))
			continue;

		UHoudiniInputObject* const InputObj = CreateTypedInputObject(NewActor, GetOuter(), NewActor->GetActorNameOrLabel(), InSettings);
		if (!IsValid(InputObj))
			continue;
		TrackedActorObjects.Emplace(NewActor, InputObj);
		NumActorsAddedLastUpdate++;
	}
}


bool UHoudiniInputLevelInstance::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	if (!InSettings.bExportLevelInstanceContent)
	{
		// Since we store positions as attributes, we need to update content after changes to the transform.
		return bTransformChanged;
	}

	if (NumActorsAddedLastUpdate > 0 || NumActorsRemovedLastUpdate > 0)
		return true;
	
	// If we are exporting content, we need to check the content of our tracked actors
	for (const auto& Entry : TrackedActorObjects)
	{
		UHoudiniInputObject const* const Object = Entry.Value;
		if (!IsValid(Object))
			continue;

		UHoudiniInputActor const* const ActorObject = Cast<UHoudiniInputActor>(Object);
		if (!IsValid(ActorObject))
			continue;

		if (ActorObject->HasContentChanged(InSettings))
			return true;
	}

	return false;
}

bool UHoudiniInputLevelInstance::HasTransformChanged() const
{
	if (bTransformChanged)
		return true;
	
	for (const auto& Entry : TrackedActorObjects)
	{
		UHoudiniInputObject const* const Object = Entry.Value;
		if (!IsValid(Object))
			continue;

		if (Object->HasTransformChanged())
			return true;
	}

	return false;
}

void UHoudiniInputPackedLevelActor::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	UBlueprint* BPObject = nullptr;
	APackedLevelActor const* PackedLevelActor = GetPackedLevelActor();

#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (IsValid(PackedLevelActor))
		BPObject = PackedLevelActor->GetRootBlueprint();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	if (IsValid(PackedLevelActor))
	{
		// From UE 5.3 APackedLevelActor::GetRootBlueprint()
		UClass* Class = PackedLevelActor->GetClass();
		while (Class->GetSuperClass() && !Class->GetSuperClass()->IsNative())
		{
			Class = Class->GetSuperClass();
		}

		BPObject = Cast<UBlueprint>(Class->ClassGeneratedBy);
	}
#else
	if (IsValid(PackedLevelActor))
		BPObject = PackedLevelActor->BlueprintAsset.LoadSynchronous();
#endif
#endif
	
	if (!IsValid(BPObject))
	{
		BlueprintInputObject = nullptr;
		return;
	}

	if (!IsValid(BlueprintInputObject) || BPObject != BlueprintInputObject->GetBlueprint())
	{
		BlueprintInputObject = Cast<UHoudiniInputBlueprint>(CreateTypedInputObject(BPObject, this, BPObject->GetName(), InSettings));
		return;
	}

	BlueprintInputObject->Update(BPObject, InSettings);
}

bool UHoudiniInputPackedLevelActor::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	if (Super::HasContentChanged(InSettings))
		return true;

	if (!IsValid(BlueprintInputObject))
		return false;

	return BlueprintInputObject->HasContentChanged(InSettings);
}

APackedLevelActor* UHoudiniInputPackedLevelActor::GetPackedLevelActor() const
{
	return Cast<APackedLevelActor>(InputObject.LoadSynchronous());
}

void UHoudiniInputPackedLevelActor::InvalidateData()
{
	// Invalidate BP input object
	if (IsValid(BlueprintInputObject))
		BlueprintInputObject->InvalidateData();

	Super::InvalidateData();
}

bool UHoudiniInputPackedLevelActor::ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const
{
	// Only track components when the old input system is being used: the old input system treats a packed level actor
	// instance like a normal actor/world input
	return false;
}

bool UHoudiniInputLandscape::ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const
{
	// We only track LandscapeComponents for landscape inputs since the Landscape tools
	// have this very interesting and creative way of adding components when the tool is activated
	// (looking at you Flatten tool) which causes cooking loops.
	if (!IsValid(InComponent))
		return false;
	
	if (InComponent->IsA(ULandscapeComponent::StaticClass()))
		return true;

	// Use InSettings if provided, otherwise use the settings cached at the last update
	const FHoudiniInputObjectSettings& Settings = InSettings ? *InSettings : CachedInputSettings;

	if (Settings.bLandscapeAutoSelectSplines)
	{
		if (InComponent->IsA(ULandscapeSplinesComponent::StaticClass()))
			return true;

		if (Settings.bLandscapeSplinesExportSplineMeshComponents && InComponent->IsA(USplineMeshComponent::StaticClass()))
			return true;
	}
	
	return false;
}

bool UHoudiniInputLandscape::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	if (Super::HasContentChanged(InSettings))
	{
		return true;
	}

	// Ignore changes to landscapes in world partition, loading and unloading landscapes will trigger cooks
	// which is very tiresome.
	if (GetLandscapeProxy()->GetWorld()->IsPartitionedWorld())
		return false;

	const int32 NumComponents = CountLandscapeComponents();
	return NumComponents != CachedNumLandscapeComponents;
}

FTransform
UHoudiniInputLandscape::GetHoudiniObjectTransform() const
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	FTransform T = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(LandscapeProxy);
	return T;
}

void
UHoudiniInputLandscape::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	// Get the current value for if exporting spline meshes is enabled
	if (!InSettings.bLandscapeSplinesExportSplineMeshComponents || !InSettings.bMergeSplineMeshComponents)
	{
		// Invalidate the merged splines nodes
		InvalidateSplinesMeshData();
	}
	
	Super::Update(InObject, InSettings);

	ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InObject);
	if (LandscapeProxy)
	{
		Transform = LandscapeProxy->GetActorTransform();
		CachedNumLandscapeComponents = CountLandscapeComponents();
	}
}

void UHoudiniInputLandscape::MarkTransformChanged(const bool bInChanged)
{
	// Landscape transforms aren't just set on one node in Houdini,
	// so if the actor transform has changed, update all nodes.
	MarkChanged(true);	
}

bool UHoudiniInputLandscape::HasActorTransformChanged() const
{
	if (!GetActor())
		return false;

	if (HasComponentsTransformChanged())
		return true;

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (IsValid(LandscapeProxy))
	{
		const FTransform ActorTransform = LandscapeProxy->GetActorTransform();
		if (!Transform.Equals(ActorTransform))
		{
			return true;
		}
	}

	return false;
}

bool
UHoudiniInputBlueprint::GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects)
{
	if (Super::GetChangedObjectsAndValidNodes(OutChangedObjects, OutNodeIdsOfUnchangedValidObjects))
		return true;

	bool bAnyChanges = false;
	return bAnyChanges;
}

bool 
UHoudiniInputBlueprint::HasComponentsTransformChanged() const
{
	// Search through all the child components to see if we have changed transforms in there.
	for (auto CurrentComp : GetComponents())
	{
		if (CurrentComp->HasComponentTransformChanged())
		{
			return true;
		}
	}

	return false;
}

bool
UHoudiniInputBlueprint::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	// In the new input system, treat the input blueprint as changed if it has any component changes

	{
		if (GetLastUpdateNumComponentsAdded() > 0 || GetLastUpdateNumComponentsRemoved() > 0)
			return true;
		for (UHoudiniInputSceneComponent const* const Component : GetComponents())
		{
			if (!IsValid(Component))
				continue;
			if (Component->HasComponentChanged(InSettings))
				return true;
		}
	}

	return false;
}

void
UHoudiniInputBlueprint::InvalidateData()
{
	// Call invalidate on input component objects
	for (auto* const CurrentComp : GetComponents())
	{
		if (!IsValid(CurrentComp))
			continue;

		CurrentComp->InvalidateData();
	}

	Super::InvalidateData();
}


bool
UHoudiniInputBlueprint::UsesInputObjectNode() const
{
	return true;
}

void
UHoudiniInputBlueprint::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	const bool bHasInputObjectChanged = InputObject != InObject;

	Super::Update(InObject, InSettings);

	UBlueprint* BP = Cast<UBlueprint>(InObject);
	ensure(BP);

	if (BP)
	{
		// If we are updating (InObject == InputObject), then remove stale components and add new components,
		// if InObject != InputObject, remove all components and rebuild
		if (bHasInputObjectChanged)
		{
			// The actor's components that can be sent as inputs
			LastUpdateNumComponentsRemoved = BPComponents.Num();

			BPComponents.Empty();
			BPSceneComponents.Empty();

			TArray<USceneComponent*> AllComponents;
			USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
			if (IsValid(SCS))
			{
				const TArray<USCS_Node*>& Nodes = SCS->GetAllNodes();
				for (auto& CurNode : Nodes)
				{
					if (!IsValid(CurNode))
						continue;

					UActorComponent* CurComp = CurNode->ComponentTemplate;
					if (!IsValid(CurComp))
						continue;

					USceneComponent* CurSceneComp = Cast<USceneComponent>(CurComp);
					if (!IsValid(CurSceneComp))
						continue;

					AllComponents.Add(CurSceneComp);
				}
			}

			BPComponents.Reserve(AllComponents.Num());
			for (USceneComponent* SceneComponent : AllComponents)
			{
				if (!IsValid(SceneComponent))
					continue;

				UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
					SceneComponent, GetOuter(), BP->GetName(), InSettings);
				if (!InputObj)
					continue;

				UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
				if (!SceneInput)
					continue;

				BPComponents.Add(SceneInput);
				BPSceneComponents.Add(TSoftObjectPtr<UObject>(SceneComponent));
			}
			LastUpdateNumComponentsAdded = BPComponents.Num();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;

			// Look for any components to add or remove
			TArray<USceneComponent*> NewComponents;
			USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
			if (IsValid(SCS))
			{
				const TArray<USCS_Node*>& Nodes = SCS->GetAllNodes();
				for (auto& CurNode : Nodes)
				{
					if (!IsValid(CurNode))
						continue;

					UActorComponent* CurComp = CurNode->ComponentTemplate;
					if (!IsValid(CurComp))
						continue;

					USceneComponent* CurSceneComp = Cast<USceneComponent>(CurComp);
					if (!IsValid(CurSceneComp))
						continue;

					if (!BPSceneComponents.Contains(CurSceneComp))
					{
						NewComponents.Add(CurSceneComp);
					}
				}
			}

			// Update the BP input components (from the same actor)
			TArray<int32> ComponentIndicesToRemove;
			const int32 NumActorComponents = BPComponents.Num();
			for (int32 Index = 0; Index < NumActorComponents; ++Index)
			{
				UHoudiniInputSceneComponent* CurBPComp = BPComponents[Index];
				if (!IsValid(CurBPComp))
				{
					ComponentIndicesToRemove.Add(Index);
					continue;
				}

				// Does the component still exist on the BP?
				UObject* const CompObj = CurBPComp->GetObject();
				// Make sure the BP is still valid
				if (!IsValid(CompObj))
				{
					// If it's not, mark it for deletion
					if ((CurBPComp->GetInputNodeId() > 0) || (CurBPComp->GetInputObjectNodeId() > 0))
					{
						CurBPComp->InvalidateData();
					}

					ComponentIndicesToRemove.Add(Index);
					continue;
				}
			}

			// Remove the destroyed/invalid components
			const int32 NumToRemove = ComponentIndicesToRemove.Num();
			if (NumToRemove > 0)
			{
				for (int32 Index = NumToRemove - 1; Index >= 0; --Index)
				{
					const int32& IndexToRemove = ComponentIndicesToRemove[Index];

					UHoudiniInputSceneComponent* const CurBPComp = BPComponents[IndexToRemove];
					if (CurBPComp)
						BPSceneComponents.Remove(CurBPComp->InputObject);

					const bool bAllowShrink = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
					BPComponents.RemoveAtSwap(IndexToRemove, 1, bAllowShrink ? EAllowShrinking::Yes : EAllowShrinking::No);
#else
					BPComponents.RemoveAtSwap(IndexToRemove, 1, bAllowShrink);
#endif

					LastUpdateNumComponentsRemoved++;
				}
			}

			// Call Update on the remaining scene components (new components have Update called in CreateTypeInputObject below)
			for (UHoudiniInputSceneComponent* const ComponentInputObject : BPComponents)
			{
				if (!IsValid(ComponentInputObject))
					continue;
				USceneComponent* const SceneComponent = ComponentInputObject->GetSceneComponent();
				ComponentInputObject->Update(SceneComponent, InSettings);
			}

			if (NewComponents.Num() > 0)
			{
				for (USceneComponent* SceneComponent : NewComponents)
				{
					if (!IsValid(SceneComponent))
						continue;

					UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
						SceneComponent, GetOuter(), BP->GetName(), InSettings);
					if (!InputObj)
						continue;

					UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
					if (!SceneInput)
						continue;

					BPComponents.Add(SceneInput);
					BPSceneComponents.Add(SceneComponent);

					LastUpdateNumComponentsAdded++;
				}
			}

			if (LastUpdateNumComponentsAdded > 0 || LastUpdateNumComponentsRemoved > 0)
			{
				BPComponents.Shrink();
			}
		}
	}
	else
	{
		// If we don't have a valid BP or null, delete any input components we still have and mark as changed
		if (BPComponents.Num() > 0)
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = BPComponents.Num();
			BPComponents.Empty();
			BPSceneComponents.Empty();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;
		}
	}
}


EHoudiniInputObjectType
UHoudiniInputObject::GetInputObjectTypeFromObject(UObject* InObject)
{
	if (InObject->IsA(USceneComponent::StaticClass()))
	{
		// Handle component inputs
		if (InObject->IsA(UInstancedStaticMeshComponent::StaticClass()))
		{
			// ISMC derives from SMC, so always test instances before static meshes
			return EHoudiniInputObjectType::InstancedStaticMeshComponent;
		}
		else if (InObject->IsA(USkeletalMeshComponent::StaticClass()))
		{
			// SKMC also derives from SMC, so SKMC must be tested before SMC
			return EHoudiniInputObjectType::SkeletalMeshComponent;
		}
		else if (InObject->IsA(USplineMeshComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::SplineMeshComponent;
		}
		else if (InObject->IsA(UStaticMeshComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::StaticMeshComponent;
		}
		else if (InObject->IsA(USplineComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::SplineComponent;
		}
		else if (InObject->IsA(UHoudiniSplineComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniSplineComponent;
		}
		else if (InObject->IsA(UHoudiniAssetComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniAssetComponent;
		}
		else if (InObject->IsA(UCameraComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::CameraComponent;
		}
		else if (InObject->IsA(UGeometryCollectionComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::GeometryCollectionComponent;
		}
		else if (InObject->IsA(ULandscapeSplinesComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::LandscapeSplinesComponent;
		}
		else if (InObject->IsA(UAnimSequence::StaticClass()))
		{
			return EHoudiniInputObjectType::Animation;
		}
		else
		{
			return EHoudiniInputObjectType::SceneComponent;
		}
	}
	else if (InObject->IsA(AActor::StaticClass()))
	{
		// Handle actors
		if (InObject->IsA(ALandscapeProxy::StaticClass()))
		{
			return EHoudiniInputObjectType::Landscape;
		}
		else if (InObject->IsA(APackedLevelActor::StaticClass()))
		{
			return EHoudiniInputObjectType::PackedLevelActor;
		}
		else if (InObject->IsA(ALevelInstance::StaticClass()))
		{
			return EHoudiniInputObjectType::LevelInstance;
		}
		else if (InObject->IsA(ABrush::StaticClass()))
		{
			return EHoudiniInputObjectType::Brush;
		}
		else if (InObject->IsA(AHoudiniAssetActor::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniAssetActor;
		}
		else if (InObject->IsA(ALandscapeSplineActor::StaticClass()))
		{
			return EHoudiniInputObjectType::LandscapeSplineActor;
		}
		else
		{
			return EHoudiniInputObjectType::Actor;
		}
	}
	else if (InObject->IsA(UBlueprint::StaticClass())) 
	{
		return EHoudiniInputObjectType::Blueprint;
	}
	else if (InObject->IsA(UFoliageType_InstancedStaticMesh::StaticClass())) 
	{
		return EHoudiniInputObjectType::FoliageType_InstancedStaticMesh;
	}
	else
	{
		if (InObject->IsA(UStaticMesh::StaticClass()))
		{
			return EHoudiniInputObjectType::StaticMesh;
		}
		else if (InObject->IsA(USkeletalMesh::StaticClass()))
		{
			return EHoudiniInputObjectType::SkeletalMesh;
		}
		else if (InObject->IsA(UAnimSequence::StaticClass()))
		{
			return EHoudiniInputObjectType::Animation;
		}
		else if (InObject->IsA(UGeometryCollection::StaticClass()))
		{
			return EHoudiniInputObjectType::GeometryCollection;
		}
		else if (InObject->IsA(UDataTable::StaticClass()))
		{
			return EHoudiniInputObjectType::DataTable;
		}
		else
		{
			return EHoudiniInputObjectType::Object;
		}
	}
	// unreachable
	// return EHoudiniInputObjectType::Invalid;
}



//-----------------------------------------------------------------------------------------------------------------------------
// UHoudiniInputBrush
//-----------------------------------------------------------------------------------------------------------------------------

FHoudiniBrushInfo::FHoudiniBrushInfo()
	: CachedTransform()
	, CachedOrigin(ForceInitToZero)
	, CachedExtent(ForceInitToZero)
	, CachedBrushType(EBrushType::Brush_Default)
	, CachedSurfaceHash(0)
{
}

FHoudiniBrushInfo::FHoudiniBrushInfo(ABrush* InBrushActor)
{
	if (!InBrushActor)
		return;

	BrushActor = InBrushActor;
	CachedTransform = BrushActor->GetActorTransform();
	BrushActor->GetActorBounds(false, CachedOrigin, CachedExtent);
	CachedBrushType = BrushActor->BrushType;

#if WITH_EDITOR
	UModel* Model = BrushActor->Brush;

	// Cache the hash of the surface properties
	if (IsValid(Model) && IsValid(Model->Polys))
	{
		int32 NumPolys = Model->Polys->Element.Num();
		CachedSurfaceHash = 0;
		for(int32 iPoly = 0; iPoly < NumPolys; ++iPoly)
		{
			const FPoly& Poly = Model->Polys->Element[iPoly]; 
			CombinePolyHash(CachedSurfaceHash, Poly);
		}
	}
	else
	{
		CachedSurfaceHash = 0;
	}
#endif
}

bool FHoudiniBrushInfo::HasChanged() const
{
	if (!BrushActor.IsValid())
		return false;

	// Has the transform changed?
	if (!BrushActor->GetActorTransform().Equals(CachedTransform))
		return true;

	if (BrushActor->BrushType != CachedBrushType)
		return true;

	// Has the actor bounds changed?
	FVector TmpOrigin, TmpExtent;
	BrushActor->GetActorBounds(false, TmpOrigin, TmpExtent);

	if (!(TmpOrigin.Equals(CachedOrigin) && TmpExtent.Equals(CachedExtent) ))
		return true;
#if WITH_EDITOR
	// Is there a tracked surface property that changed?
	UModel* Model = BrushActor->Brush;
	if (IsValid(Model) && IsValid(Model->Polys))
	{
		// Hash the incoming surface properties and compared it against the cached hash.
		int32 NumPolys = Model->Polys->Element.Num();
		uint64 SurfaceHash = 0;
		for (int32 iPoly = 0; iPoly < NumPolys; ++iPoly)
		{
			const FPoly& Poly = Model->Polys->Element[iPoly];
			CombinePolyHash(SurfaceHash, Poly);
		}
		if (SurfaceHash != CachedSurfaceHash)
			return true;
	}
	else
	{
		if (CachedSurfaceHash != 0)
			return true;
	}
#endif
	return false;
}

int32 FHoudiniBrushInfo::GetNumVertexIndicesFromModel(const UModel* Model)
{
	const TArray<FBspNode>& Nodes = Model->Nodes;		
	int32 NumIndices = 0;
	// Build the face counts buffer by iterating over the BSP nodes.
	for(const FBspNode& Node : Nodes)
	{
		NumIndices += Node.NumVertices;
	}
	return NumIndices;
}

UModel* UHoudiniInputBrush::GetCachedModel() const
{
	return CombinedModel;
}

bool UHoudiniInputBrush::HasBrushesChanged(const TArray<ABrush*>& InBrushes) const
{
	if (InBrushes.Num() != BrushesInfo.Num())
		return true;

	int32 NumBrushes = BrushesInfo.Num();

	for (int32 InfoIndex = 0; InfoIndex < NumBrushes; ++InfoIndex)
	{
		const FHoudiniBrushInfo& BrushInfo = BrushesInfo[InfoIndex];
		// Has the cached brush actor invalid?
		if (!BrushInfo.BrushActor.IsValid())
			return true;

		// Has there been an order change in the actors list?
		if (InBrushes[InfoIndex] != BrushInfo.BrushActor.Get())
			return true;

		// Has there been any other changes to the brush?
		if (BrushInfo.HasChanged())
			return true;
	}

	// Nothing has changed.
	return false;
}

void UHoudiniInputBrush::UpdateCachedData(UModel* InCombinedModel, const TArray<ABrush*>& InBrushes)
{
	ABrush* InputBrush = GetBrush();
	if (IsValid(InputBrush))
	{
		CachedInputBrushType = InputBrush->BrushType;
	}

	// Cache the combined model aswell as the brushes used to generate this model.
	CombinedModel = InCombinedModel;

	BrushesInfo.SetNum(InBrushes.Num());
	for (int i = 0; i < InBrushes.Num(); ++i)
	{
		if (!InBrushes[i])
			continue;
		BrushesInfo[i] = FHoudiniBrushInfo(InBrushes[i]);
	}
}


void
UHoudiniInputBrush::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	ABrush* BrushActor = GetBrush();
	if (!IsValid(BrushActor))
	{
		bIgnoreInputObject = true;
		return;
	}

	CachedInputBrushType = BrushActor->BrushType;

	bIgnoreInputObject = ShouldIgnoreThisInput();
}

bool
UHoudiniInputBrush::ShouldIgnoreThisInput()
{
	// Invalid brush, should be ignored
	ABrush* BrushActor = GetBrush();
	ensure(BrushActor);
	if (!BrushActor)
		return true;

	// If the BrushType has changed since caching this object, this object cannot be ignored.
	if (CachedInputBrushType != BrushActor->BrushType)
		return false;

	// If it's not an additive brush, we want to ignore it
	bool bShouldBeIgnored = BrushActor->BrushType != EBrushType::Brush_Add;

	// If this is not a static brush (e.g., AVolume), ignore it.
	if (!bShouldBeIgnored)
		bShouldBeIgnored = !BrushActor->IsStaticBrush();

	return bShouldBeIgnored;
}

bool UHoudiniInputBrush::HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	ABrush* BrushActor = GetBrush();
	ensure(BrushActor);
	
	if (!BrushActor)
		return false;

	if (BrushActor->BrushType != CachedInputBrushType)
		return true;
	
	if (bIgnoreInputObject)
		return false;

	// Find intersecting actors and capture their properties so that
	// we can determine whether something has changed.
	TArray<ABrush*> IntersectingBrushes;
	FindIntersectingSubtractiveBrushes(this, IntersectingBrushes);
	
	if (HasBrushesChanged(IntersectingBrushes))
	{
		return true;
	}

	return false;
}

bool
UHoudiniInputBrush::HasActorTransformChanged() const
{
	if (bIgnoreInputObject)
		return false;

	return Super::HasActorTransformChanged();
}


bool UHoudiniInputBrush::FindIntersectingSubtractiveBrushes(const UHoudiniInputBrush* InputBrush, TArray<ABrush*>& OutBrushes)
{
	TArray<AActor*> IntersectingActors;	
	TArray<FBox> Bounds;

	if (!IsValid(InputBrush))
		return false;

	ABrush* BrushActor = InputBrush->GetBrush();
	if (!IsValid(BrushActor))
		return false;


	OutBrushes.Empty();

	Bounds.Add( BrushActor->GetComponentsBoundingBox(true, true) );

	FHoudiniEngineRuntimeUtils::FindActorsOfClassInBounds(BrushActor->GetWorld(), ABrush::StaticClass(), Bounds, nullptr, IntersectingActors);

	//--------------------------------------------------------------------------------------------------
	// Filter the actors to only keep intersecting subtractive brushes.
	//--------------------------------------------------------------------------------------------------
	for (AActor* Actor : IntersectingActors)
	{
		// Filter out anything that is not a static brush (typically volume actors).
		ABrush* Brush = Cast<ABrush>(Actor);

		// NOTE: The brush actor needs to be added in the correct map/level order
		// together with the subtractive brushes otherwise the CSG operations 
		// will not match the BSP in the level.
		if (Actor == BrushActor)
			OutBrushes.Add(Brush);

		if (!(Brush && Brush->IsStaticBrush()))
			continue;

		if (Brush->BrushType == Brush_Subtract)
			OutBrushes.Add(Brush);
	}

	return true;	
}

#if WITH_EDITOR
void 
UHoudiniInputObject::PostEditUndo()
{
	Super::PostEditUndo();
	MarkChanged(true);
}
#endif

UHoudiniInputObject*
UHoudiniInputObject::DuplicateAndCopyState(UObject * DestOuter)
{
	UHoudiniInputObject* NewInput = Cast<UHoudiniInputObject>(StaticDuplicateObject(this, DestOuter));
	NewInput->CopyStateFrom(this, false);
	return NewInput;
}

void 
UHoudiniInputObject::CopyStateFrom(UHoudiniInputObject* InInput, bool bCopyAllProperties)
{
	// Copy the state of this UHoudiniInput object.
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References should be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InInput, this, Params);
	}

	InputNodeId = InInput->InputNodeId;
	InputObjectNodeId = InInput->InputObjectNodeId;
	bHasChanged = InInput->bHasChanged;
	bNeedsToTriggerUpdate = InInput->bNeedsToTriggerUpdate;
	bTransformChanged = InInput->bTransformChanged;
	Guid = InInput->Guid;
	InputNodeHandle = InInput->InputNodeHandle;

#if WITH_EDITORONLY_DATA
	bUniformScaleLocked = InInput->bUniformScaleLocked;
#endif

}

void
UHoudiniInputObject::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	bCanDeleteHoudiniNodes = bInCanDeleteNodes;
}

bool
UHoudiniInputObject::GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects)
{
	if (HasChanged())
	{
		// Has changed, needs to be recreated
		OutChangedObjects.Add(this);
		return true;
	}
	
	if (UsesInputObjectNode())
	{
		// Do not use InputObjectNodeId directly here! 
		// This can cause issues when dealing with InputActors
		int32 InObjNodeId = GetInputObjectNodeId();
		if (InObjNodeId >= 0)
		{
			// No changes, keep it
			OutNodeIdsOfUnchangedValidObjects.Add(InObjNodeId);
		}
		else
		{
			// Needs, but does not have, a current HAPI input node
			OutChangedObjects.Add(this);
			return true;
		}
	}

	// No changes and valid object node exists (or no node is used by this object type)
	return false;
}

//
UHoudiniInputDataTable::UHoudiniInputDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniInputObject *
UHoudiniInputDataTable::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_DT_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputDataTable::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputDataTable * HoudiniInputObject = NewObject<UHoudiniInputDataTable>(
		InOuter, UHoudiniInputDataTable::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::DataTable;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UDataTable*
UHoudiniInputDataTable::GetDataTable() const
{
	return Cast<UDataTable>(InputObject.LoadSynchronous());
}

UHoudiniInputFoliageType_InstancedStaticMesh::UHoudiniInputFoliageType_InstancedStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniInputObject*
UHoudiniInputFoliageType_InstancedStaticMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings)
{
	FString InputObjectNameStr = "HoudiniInputObject_FoliageSM_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputFoliageType_InstancedStaticMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputFoliageType_InstancedStaticMesh * HoudiniInputObject = NewObject<UHoudiniInputFoliageType_InstancedStaticMesh>(
		InOuter, UHoudiniInputFoliageType_InstancedStaticMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::FoliageType_InstancedStaticMesh;
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

void
UHoudiniInputFoliageType_InstancedStaticMesh::Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings)
{
	UHoudiniInputObject::Update(InObject, InSettings);
	UFoliageType_InstancedStaticMesh* const FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InObject);
	ensure(FoliageType);
	ensure(FoliageType->GetStaticMesh());
}

UStaticMesh*
UHoudiniInputFoliageType_InstancedStaticMesh::GetStaticMesh() const
{
	if (!InputObject.IsValid())
		return nullptr;
	
	UFoliageType_InstancedStaticMesh* const FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InputObject.LoadSynchronous());
	if (!IsValid(FoliageType))
		return nullptr;

	return FoliageType->GetStaticMesh();
}


UHoudiniInputSplineMeshComponent::UHoudiniInputSplineMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedForwardAxis()
	, CachedSplineUpDir(FVector::UpVector)
	, CachedSplineBoundaryMax(0)
	, CachedSplineBoundaryMin(0)
	, CachedbSmoothInterpRollScale(false)
{

}

UHoudiniInputObject*
UHoudiniInputSplineMeshComponent::Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor)
{
	const FString InputObjectNameStr = "HoudiniInputObject_SplineMeshComponent_" + InName;
	const FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSplineMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSplineMeshComponent* HoudiniInputObject = NewObject<UHoudiniInputSplineMeshComponent>(
		InOuter, UHoudiniInputSplineMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->SetParentInputActor(InParentInputActor);
	HoudiniInputObject->Type = EHoudiniInputObjectType::SplineMeshComponent;
	HoudiniInputObject->MeshPackageGuid = FGuid::NewGuid();
	HoudiniInputObject->Update(InObject, InInputSettings);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

void
UHoudiniInputSplineMeshComponent::Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings)
{
	Super::Update(InObject, InSettings);

	USplineMeshComponent const* const SplineMeshComponent = Cast<USplineMeshComponent>(InObject);
	ensure(SplineMeshComponent);

	CachedForwardAxis = SplineMeshComponent->ForwardAxis;
	CachedSplineParams = SplineMeshComponent->SplineParams;
	CachedSplineUpDir = SplineMeshComponent->SplineUpDir;
	CachedSplineBoundaryMax = SplineMeshComponent->SplineBoundaryMax;
	CachedSplineBoundaryMin = SplineMeshComponent->SplineBoundaryMin;
	CachedbSmoothInterpRollScale = SplineMeshComponent->bSmoothInterpRollScale;
}

USplineMeshComponent*
UHoudiniInputSplineMeshComponent::GetSplineMeshComponent() const
{
	return Cast<USplineMeshComponent>(InputObject.LoadSynchronous());
}

bool
UHoudiniInputSplineMeshComponent::HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const
{
	if (Super::HasComponentChanged(InSettings))
	{
		return true;
	}
	
	USplineMeshComponent const* const SplineMeshComponent = Cast<USplineMeshComponent>(InputObject.LoadSynchronous());

	if (!IsValid(SplineMeshComponent))
		return false;

	if (SplineMeshComponent->ForwardAxis != CachedForwardAxis
		|| SplineMeshComponent->SplineUpDir != CachedSplineUpDir
		|| SplineMeshComponent->SplineBoundaryMax != CachedSplineBoundaryMax
		|| SplineMeshComponent->SplineBoundaryMin != CachedSplineBoundaryMin
		|| SplineMeshComponent->bSmoothInterpRollScale != CachedbSmoothInterpRollScale)
	{
		return true;
	}

	if (SplineMeshComponent->SplineParams.StartPos != CachedSplineParams.StartPos
			|| SplineMeshComponent->SplineParams.StartTangent != CachedSplineParams.StartTangent
			|| SplineMeshComponent->SplineParams.StartScale != CachedSplineParams.StartScale
			|| SplineMeshComponent->SplineParams.StartRoll != CachedSplineParams.StartRoll
			|| SplineMeshComponent->SplineParams.StartOffset != CachedSplineParams.StartOffset
			|| SplineMeshComponent->SplineParams.EndPos != CachedSplineParams.EndPos
			|| SplineMeshComponent->SplineParams.EndScale != CachedSplineParams.EndScale
			|| SplineMeshComponent->SplineParams.EndTangent != CachedSplineParams.EndTangent
			|| SplineMeshComponent->SplineParams.EndRoll != CachedSplineParams.EndRoll
			|| SplineMeshComponent->SplineParams.EndOffset != CachedSplineParams.EndOffset)
	{
		return true;
	}

	return false;
}
