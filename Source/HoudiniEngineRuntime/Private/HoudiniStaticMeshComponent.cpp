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

#include "HoudiniStaticMeshComponent.h"

#include "Components/BillboardComponent.h"
#include "Engine/CollisionProfile.h"
#include "SceneInterface.h"
#include "Engine/Texture2D.h" 

#include "HoudiniStaticMesh.h"
#include "HoudiniStaticMeshSceneProxy.h"


UHoudiniStaticMeshComponent::UHoudiniStaticMeshComponent(const FObjectInitializer &InInitialzer) :
	Super(InInitialzer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Mesh = nullptr;
	bHoudiniIconVisible = true;

#if WITH_EDITOR
	bVisualizeComponent = true;
#endif
}

void UHoudiniStaticMeshComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	if (bVisualizeComponent && SpriteComponent != nullptr && GetOwner())
	{
		SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/HoudiniEngine/Textures/icon_houdini_logo_128.icon_houdini_logo_128")));
		UpdateSpriteComponent();
	}
#endif
}

//void
//UHoudiniStaticMeshComponent::PostLoad()
//{
//	Super::PostLoad();
//
//	//NotifyMeshUpdated();
//}

void UHoudiniStaticMeshComponent::SetMesh(UHoudiniStaticMesh *InMesh)
{
	if (Mesh == InMesh)
		return;

	Mesh = InMesh; 
	NotifyMeshUpdated();
}

FPrimitiveSceneProxy* UHoudiniStaticMeshComponent::CreateSceneProxy()
{
	check(SceneProxy == nullptr);

	FHoudiniStaticMeshSceneProxy* NewProxy = nullptr;
	if (Mesh && Mesh->GetNumTriangles() > 0)
	{
		NewProxy = new FHoudiniStaticMeshSceneProxy(this, GetScene()->GetFeatureLevel());
		NewProxy->Build();
	}
	return NewProxy;
}

FBoxSphereBounds UHoudiniStaticMeshComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	if (Mesh)
	{
		// mesh bounds
		FBoxSphereBounds NewBounds = LocalBounds.TransformBy(InLocalToWorld);
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(InLocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

#if WITH_EDITOR
void UHoudiniStaticMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName NAME_HoudiniIconVisible(TEXT("bHoudiniIconVisible"));
	const FName NAME_Mesh(TEXT("Mesh"));
	const FName NAME_PropertyChanged = PropertyChangedEvent.GetPropertyName();
	if (NAME_PropertyChanged == NAME_HoudiniIconVisible)
	{
		UpdateSpriteComponent();
	}
	else if (NAME_PropertyChanged == NAME_Mesh)
	{
		NotifyMeshUpdated();
	}
}
#endif

void UHoudiniStaticMeshComponent::SetHoudiniIconVisible(bool bInHoudiniIconVisible) 
{ 
	bHoudiniIconVisible = bInHoudiniIconVisible; 
#if WITH_EDITORONLY_DATA
	UpdateSpriteComponent();
#endif
}

void UHoudiniStaticMeshComponent::NotifyMeshUpdated()
{
	MarkRenderStateDirty();
	if (Mesh)
	{
		LocalBounds = Mesh->CalcBounds();
	}
	else
	{
		LocalBounds.Init();
	}

	UpdateBounds();

#if WITH_EDITORONLY_DATA
	UpdateSpriteComponent();
#endif
}

#if WITH_EDITORONLY_DATA
void UHoudiniStaticMeshComponent::UpdateSpriteComponent()
{
	if (SpriteComponent)
	{
		const FBoxSphereBounds B = Bounds.TransformBy(GetComponentTransform().ToInverseMatrixWithScale());
		SpriteComponent->SetRelativeLocation(B.Origin + FVector(0, 0, B.BoxExtent.Size()));
		SpriteComponent->SetVisibility(bHoudiniIconVisible);
	}
}
#endif

int32 UHoudiniStaticMeshComponent::GetNumMaterials() const
{
	// From UStaticMesh:
	// @note : you don't have to consider Materials.Num()
	// that only counts if overridden and it can't be more than GetStaticMesh()->Materials. 
	if (Mesh)
	{
		return Mesh->GetNumStaticMaterials();
	}
	else
	{
		return 0;
	}
}

int32 UHoudiniStaticMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	return Mesh ? Mesh->GetMaterialIndex(MaterialSlotName) : -1;
}

TArray<FName> UHoudiniStaticMeshComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	if (Mesh)
	{
		const TArray<FStaticMaterial> &StaticMaterials = Mesh->GetStaticMaterials();
		const int32 NumMaterials = StaticMaterials.Num();
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			const FStaticMaterial &StaticMaterial = StaticMaterials[MaterialIndex];
			MaterialNames.Add(StaticMaterial.MaterialSlotName);
		}
	}
	return MaterialNames;
}

bool UHoudiniStaticMeshComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) >= 0;
}

UMaterialInterface* UHoudiniStaticMeshComponent::GetMaterial(int32 MaterialIndex) const
{
	// From UStaticMesh:
	// If we have a base materials array, use that
	if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex];
	}
	// Otherwise get from static mesh
	else
	{
		return Mesh ? Mesh->GetMaterial(MaterialIndex) : nullptr;
	}
}

