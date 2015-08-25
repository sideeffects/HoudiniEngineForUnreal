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
	HoudiniAssetComponent(nullptr),
	InstancePathName(TEXT("")),		
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
	InputField->RotationOffsets = OtherInputField->RotationOffsets;
	InputField->ScaleOffsets = OtherInputField->ScaleOffsets;
	InputField->bScaleOffsetsLinearlyArray = OtherInputField->bScaleOffsetsLinearlyArray;
	InputField->InstancedTransforms = OtherInputField->InstancedTransforms;
	InputField->StaticMeshes = OtherInputField->StaticMeshes;
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
	Ar << RotationOffsets;
	Ar << ScaleOffsets;
	Ar << bScaleOffsetsLinearlyArray;

	Ar << InstancedTransforms;
	Ar << VariationTransformsArray;
	Ar << InstancedStaticMeshComponents;	
	Ar << StaticMeshes;    
	Ar << OriginalStaticMesh;
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

		for (int32 Idx = 0; Idx < HoudiniAssetInstanceInputField->StaticMeshes.Num(); ++Idx)
		{
			UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->StaticMeshes[Idx];
			Collector.AddReferencedObject(StaticMesh, InThis);
		}
		
		for (int32 Idx = 0; Idx < HoudiniAssetInstanceInputField->InstancedStaticMeshComponents.Num(); ++Idx)
		{
			UInstancedStaticMeshComponent* InstancedStaticMeshComponent = HoudiniAssetInstanceInputField->InstancedStaticMeshComponents[Idx];            
			Collector.AddReferencedObject(InstancedStaticMeshComponent, InThis);
		}
		
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInstanceInputField::BeginDestroy()
{

	for (int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent
			= InstancedStaticMeshComponents[Idx];
		if (InstancedStaticMeshComponent)
		{
			InstancedStaticMeshComponent->UnregisterComponent();
			InstancedStaticMeshComponent->DetachFromParent();
			InstancedStaticMeshComponent->DestroyComponent();

			if (HoudiniAssetComponent)
			{
				HoudiniAssetComponent->AttachChildren.Remove(InstancedStaticMeshComponent);
			}
		}
	}

	Super::BeginDestroy();
}


void
UHoudiniAssetInstanceInputField::PostEditUndo()
{
	Super::PostEditUndo();

	int32 VariationCount = InstanceVariationCount();
	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent
			= InstancedStaticMeshComponents.Num() ? InstancedStaticMeshComponents[Idx] : NULL;

		if (InstancedStaticMeshComponent)
		{
			UStaticMesh* StaticMesh = StaticMeshes[Idx];
			InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
		}
		
	}

	UpdateInstanceTransforms();

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


void
UHoudiniAssetInstanceInputField::CreateInstancedComponent(int32 VariationIdx)
{
	check(StaticMeshes.Num() > 0 && (VariationIdx < StaticMeshes.Num()));
	
	UStaticMesh* StaticMesh = StaticMeshes[VariationIdx];
	check(HoudiniAssetComponent);
	
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = NewObject<UInstancedStaticMeshComponent>(HoudiniAssetComponent->GetOwner(),
		UInstancedStaticMeshComponent::StaticClass(), NAME_None);
	
	InstancedStaticMeshComponents.Insert(InstancedStaticMeshComponent,VariationIdx);	

	// Assign static mesh to this instanced component.
	
	InstancedStaticMeshComponents[VariationIdx]->SetStaticMesh(StaticMesh);
	InstancedStaticMeshComponents[VariationIdx]->AttachTo(HoudiniAssetComponent);
	InstancedStaticMeshComponents[VariationIdx]->RegisterComponent();
	InstancedStaticMeshComponents[VariationIdx]->GetBodyInstance()->bAutoWeld = false;

	// We want to make this invisible if it's a collision instancer.
	InstancedStaticMeshComponents[VariationIdx]->SetVisibility(!HoudiniGeoPartObject.bIsCollidable);
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
	int32 NumInstancTransforms = InstancedTransforms.Num();
	int32 VariationCount = InstanceVariationCount();

	//clear the previous cached transform assignments.
	for (int32 Idx = 0; Idx < VariationTransformsArray.Num(); Idx++)
	{
		VariationTransformsArray[Idx].Empty();
	}
	VariationTransformsArray.Empty();

	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		TArray<FTransform> VariationTransforms;
		VariationTransformsArray.Add(VariationTransforms);
	}

	for (int32 Idx = 0; Idx < NumInstancTransforms; Idx++)
	{
		FTransform Xform = InstancedTransforms[Idx];
		int32 VariationIndex = rand() % VariationCount;
		VariationTransformsArray[VariationIndex].Add(Xform);
	}

	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
		FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(
			InstancedStaticMeshComponent, 
			VariationTransformsArray[Idx],
			RotationOffsets[Idx], ScaleOffsets[Idx]);

	}

	
}


void
UHoudiniAssetInstanceInputField::UpdateRelativeTransform()
{
	int32 VariationCount = InstanceVariationCount();
	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		InstancedStaticMeshComponents[Idx]->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
	}
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
UHoudiniAssetInstanceInputField::GetInstanceVariation( int32 VariationIndex ) const
{
	if ( VariationIndex < 0 || VariationIndex >= StaticMeshes.Num() )
		return NULL;
	
	UStaticMesh* StaticMesh = StaticMeshes[VariationIndex];
	return StaticMesh;
}


void 
UHoudiniAssetInstanceInputField::AddInstanceVariation(UStaticMesh * InStaticMesh, int32 VariationIdx)
{
	check(InStaticMesh);
	
	StaticMeshes.Insert(InStaticMesh,VariationIdx);
	RotationOffsets.Insert(FRotator(0, 0, 0),VariationIdx);
	ScaleOffsets.Insert(FVector(1, 1, 1),VariationIdx);
	bScaleOffsetsLinearlyArray.Insert(true,VariationIdx);

	// Create instanced component.	
	CreateInstancedComponent( VariationIdx );
}

void
UHoudiniAssetInstanceInputField::ReplaceInstanceVariation(
								UStaticMesh * InStaticMesh,
								int Index)
{
	check(InStaticMesh);
	check(Index >= 0 && Index < StaticMeshes.Num());	
	check(InstancedStaticMeshComponents.Num() == StaticMeshes.Num());

	StaticMeshes[Index] = InStaticMesh;
	InstancedStaticMeshComponents[Index]->SetStaticMesh(InStaticMesh);
	UpdateInstanceTransforms();
}

void 
UHoudiniAssetInstanceInputField::FindStaticMeshIndices(
							UStaticMesh * InStaticMesh, 
							TArray<int> & Indices)
{
	for (int32 Idx = 0; Idx < StaticMeshes.Num(); ++Idx)
	{
		UStaticMesh* StaticMesh = StaticMeshes[Idx];
		if (StaticMesh == InStaticMesh)
		{
			Indices.Add(Idx);
		}
	}	
}

int32 
UHoudiniAssetInstanceInputField::InstanceVariationCount()
{
	return StaticMeshes.Num();
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
UHoudiniAssetInstanceInputField::GetRotationOffset(int32 VariationIdx) const
{
	return RotationOffsets[VariationIdx];
}


void
UHoudiniAssetInstanceInputField::SetRotationOffset(const FRotator& Rotator,
												   int32 VariationIdx)
{
	RotationOffsets[VariationIdx] = Rotator;
}


const FVector&
UHoudiniAssetInstanceInputField::GetScaleOffset(int32 VariationIdx) const
{
	return ScaleOffsets[VariationIdx];
}


void
UHoudiniAssetInstanceInputField::SetScaleOffset(const FVector& InScale,
												int32 VariationIdx)
{
	ScaleOffsets[VariationIdx] = InScale;
}


bool
UHoudiniAssetInstanceInputField::AreOffsetsScaledLinearly(int32 VariationIdx) const
{
	return bScaleOffsetsLinearlyArray[VariationIdx];
}


void
UHoudiniAssetInstanceInputField::SetLinearOffsetScale(bool bEnabled,
													  int32 VariationIdx)
{
	bScaleOffsetsLinearlyArray[VariationIdx] = bEnabled;
}


bool
UHoudiniAssetInstanceInputField::IsOriginalStaticMeshUsed( int32 VariationIdx ) const
{
	check(VariationIdx >= 0 && VariationIdx < StaticMeshes.Num());
	UStaticMesh* StaticMesh = StaticMeshes[VariationIdx];
	return OriginalStaticMesh == StaticMesh;
}


UInstancedStaticMeshComponent*
UHoudiniAssetInstanceInputField::GetInstancedStaticMeshComponent(int32 VariationIdx) const
{
	check(VariationIdx >= 0 && VariationIdx < InstancedStaticMeshComponents.Num());
	return InstancedStaticMeshComponents[VariationIdx];
}


const TArray<FTransform>&
UHoudiniAssetInstanceInputField::GetInstancedTransforms( int32 VariationIdx ) const
{
	check(VariationIdx >= 0 && VariationIdx < VariationTransformsArray.Num());
	return VariationTransformsArray[VariationIdx];
}


void
UHoudiniAssetInstanceInputField::RecreateRenderState()
{
	check(InstancedStaticMeshComponents.Num() == StaticMeshes.Num());
	int32 VariationCount = InstanceVariationCount();
	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		InstancedStaticMeshComponents[Idx]->RecreateRenderState_Concurrent();
	}
	
}


void
UHoudiniAssetInstanceInputField::RecreatePhysicsState()
{
	check(InstancedStaticMeshComponents.Num() == StaticMeshes.Num());
	int32 VariationCount = InstanceVariationCount();
	for (int32 Idx = 0; Idx < VariationCount; Idx++)
	{
		InstancedStaticMeshComponents[Idx]->RecreatePhysicsState();
	}
}
