/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetInstanceInputField.h"


bool
FHoudiniAssetInstanceInputFieldSortPredicate::operator()(const UHoudiniAssetInstanceInputField& A, 
	const UHoudiniAssetInstanceInputField& B) const
{
	FHoudiniGeoPartObjectSortPredicate HoudiniGeoPartObjectSortPredicate;
	return HoudiniGeoPartObjectSortPredicate(A.GetHoudiniGeoPartObject(), B.GetHoudiniGeoPartObject());
}


UHoudiniAssetInstanceInputField::UHoudiniAssetInstanceInputField(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	OriginalStaticMesh(nullptr),
	StaticMesh(nullptr),
	InstancedStaticMeshComponent(nullptr),
	HoudiniAssetComponent(nullptr),
	InstancePathName(TEXT("")),
	RotationOffset(0.0f, 0.0f, 0.0f),
	ScaleOffset(1.0f, 1.0f, 1.0f),
	bScaleOffsetsLinearly(true),
	HoudiniAssetInstanceInputFieldFlagsPacked(0)
{

}


UHoudiniAssetInstanceInputField::~UHoudiniAssetInstanceInputField()
{

}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInputField::Create(UHoudiniAssetComponent* HoudiniAssetComponent, 
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const FString& InstancePathName)
{
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField
		= NewObject<UHoudiniAssetInstanceInputField>(HoudiniAssetComponent, 
			UHoudiniAssetInstanceInputField::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetInstanceInputField->HoudiniGeoPartObject = HoudiniGeoPartObject;
	HoudiniAssetInstanceInputField->HoudiniAssetComponent = HoudiniAssetComponent;
	HoudiniAssetInstanceInputField->InstancePathName = InstancePathName;

	return HoudiniAssetInstanceInputField;
}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInputField::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, 
	UHoudiniAssetInstanceInputField* OtherInputField)
{
	UHoudiniAssetInstanceInputField* InputField
		= NewObject<UHoudiniAssetInstanceInputField>(InHoudiniAssetComponent, 
			UHoudiniAssetInstanceInputField::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	InputField->HoudiniGeoPartObject = OtherInputField->HoudiniGeoPartObject;
	InputField->HoudiniAssetComponent = InHoudiniAssetComponent;
	InputField->InstancePathName = OtherInputField->InstancePathName;
	InputField->RotationOffset = OtherInputField->RotationOffset;
	InputField->ScaleOffset = OtherInputField->ScaleOffset;
	InputField->bScaleOffsetsLinearly = OtherInputField->bScaleOffsetsLinearly;
	InputField->InstancedTransforms = OtherInputField->InstancedTransforms;
	InputField->StaticMesh = OtherInputField->StaticMesh;
	InputField->OriginalStaticMesh = OtherInputField->OriginalStaticMesh;

	return InputField;
}


void
UHoudiniAssetInstanceInputField::Serialize(FArchive& Ar)
{
	// Call base implementation first.
	Super::Serialize(Ar);

	Ar << HoudiniAssetInstanceInputFieldFlagsPacked;
	HoudiniGeoPartObject.Serialize(Ar);

	Ar << InstancePathName;
	Ar << RotationOffset;
	Ar << ScaleOffset;
	Ar << bScaleOffsetsLinearly;

	Ar << InstancedTransforms;
	Ar << InstancedStaticMeshComponent;
	Ar << StaticMesh;
	Ar << OriginalStaticMesh;

	if(Ar.IsLoading() && Ar.IsTransacting())
	{
		InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
	}
}


void
UHoudiniAssetInstanceInputField::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = Cast<UHoudiniAssetInstanceInputField>(InThis);
	if(HoudiniAssetInstanceInputField)// && !HoudiniAssetInstanceInputField->IsPendingKill())
	{
		if(HoudiniAssetInstanceInputField->OriginalStaticMesh)
		{
			Collector.AddReferencedObject(HoudiniAssetInstanceInputField->OriginalStaticMesh, InThis);
		}

		if(HoudiniAssetInstanceInputField->StaticMesh)
		{
			Collector.AddReferencedObject(HoudiniAssetInstanceInputField->StaticMesh, InThis);
		}

		if(HoudiniAssetInstanceInputField->InstancedStaticMeshComponent)
		{
			Collector.AddReferencedObject(HoudiniAssetInstanceInputField->InstancedStaticMeshComponent, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInstanceInputField::BeginDestroy()
{
	if(InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->UnregisterComponent();
		InstancedStaticMeshComponent->DetachFromParent();
		InstancedStaticMeshComponent->DestroyComponent();

		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->AttachChildren.Remove(InstancedStaticMeshComponent);
		}
	}

	Super::BeginDestroy();
}


void
UHoudiniAssetInstanceInputField::PostEditUndo()
{
	Super::PostEditUndo();

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


void
UHoudiniAssetInstanceInputField::CreateInstancedComponent()
{
	check(StaticMesh);
	check(HoudiniAssetComponent);

	if(!InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent = NewObject<UInstancedStaticMeshComponent>(HoudiniAssetComponent->GetOwner(),
			UInstancedStaticMeshComponent::StaticClass(), NAME_None);
	}

	// Assign static mesh to this instanced component.
	
	InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
	InstancedStaticMeshComponent->AttachTo(HoudiniAssetComponent);
	InstancedStaticMeshComponent->RegisterComponent();
	InstancedStaticMeshComponent->GetBodyInstance()->bAutoWeld = false;

	// We want to make this invisible if it's a collision instancer.
	InstancedStaticMeshComponent->SetVisibility(!HoudiniGeoPartObject.bIsCollidable);
}


void
UHoudiniAssetInstanceInputField::SetInstanceTransforms(const TArray<FTransform>& ObjectTransforms)
{
	InstancedTransforms = ObjectTransforms;
	UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInputField::UpdateInstanceTransforms()
{
	FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(InstancedStaticMeshComponent, InstancedTransforms,
		RotationOffset, ScaleOffset);
}


void
UHoudiniAssetInstanceInputField::UpdateRelativeTransform()
{
	InstancedStaticMeshComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
}


const FHoudiniGeoPartObject&
UHoudiniAssetInstanceInputField::GetHoudiniGeoPartObject() const
{
	return HoudiniGeoPartObject;
}


UStaticMesh*
UHoudiniAssetInstanceInputField::GetOriginalStaticMesh() const
{
	return OriginalStaticMesh;
}


UStaticMesh*
UHoudiniAssetInstanceInputField::GetStaticMesh() const
{
	return StaticMesh;
}


void
UHoudiniAssetInstanceInputField::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	check(InStaticMesh);
	StaticMesh = InStaticMesh;

	InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
}


void
UHoudiniAssetInstanceInputField::AssignThumbnailBorder(TSharedPtr<SBorder> InThumbnailBorder)
{
	ThumbnailBorder = InThumbnailBorder;
}


TSharedPtr<SBorder>
UHoudiniAssetInstanceInputField::GetThumbnailBorder() const
{
	return ThumbnailBorder;
}


void
UHoudiniAssetInstanceInputField::AssignComboButton(TSharedPtr<SComboButton> InComboButton)
{
	StaticMeshComboButton = InComboButton;
}


TSharedPtr<SComboButton>
UHoudiniAssetInstanceInputField::GetComboButton() const
{
	return StaticMeshComboButton;
}


const FRotator&
UHoudiniAssetInstanceInputField::GetRotationOffset() const
{
	return RotationOffset;
}


void
UHoudiniAssetInstanceInputField::SetRotationOffset(const FRotator& Rotator)
{
	RotationOffset = Rotator;
}


const FVector&
UHoudiniAssetInstanceInputField::GetScaleOffset() const
{
	return ScaleOffset;
}


void
UHoudiniAssetInstanceInputField::SetScaleOffset(const FVector& InScale)
{
	ScaleOffset = InScale;
}


bool
UHoudiniAssetInstanceInputField::AreOffsetsScaledLinearly() const
{
	return bScaleOffsetsLinearly;
}


void
UHoudiniAssetInstanceInputField::SetLinearOffsetScale(bool bEnabled)
{
	bScaleOffsetsLinearly = bEnabled;
}


bool
UHoudiniAssetInstanceInputField::IsOriginalStaticMeshUsed() const
{
	return OriginalStaticMesh == StaticMesh;
}


UInstancedStaticMeshComponent*
UHoudiniAssetInstanceInputField::GetInstancedStaticMeshComponent() const
{
	return InstancedStaticMeshComponent;
}


const TArray<FTransform>&
UHoudiniAssetInstanceInputField::GetInstancedTransforms() const
{
	return InstancedTransforms;
}


void
UHoudiniAssetInstanceInputField::RecreateRenderState()
{
	if(InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->RecreateRenderState_Concurrent();
	}
}


void
UHoudiniAssetInstanceInputField::RecreatePhysicsState()
{
	if(InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->RecreatePhysicsState();
	}
}
