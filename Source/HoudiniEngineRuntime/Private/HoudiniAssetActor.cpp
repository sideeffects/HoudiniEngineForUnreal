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

#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniPDGAssetLink.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

AHoudiniAssetActor::AHoudiniAssetActor(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCanBeDamaged(false);
	//PrimaryActorTick.bCanEverTick = true;
	//PrimaryActorTick.bStartWithTickEnabled = true;

	// Create Houdini component and attach it to a root component.
	HoudiniAssetComponent =
		ObjectInitializer.CreateDefaultSubobject< UHoudiniAssetComponent >(this, TEXT("HoudiniAssetComponent"));

	//HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	RootComponent = HoudiniAssetComponent;
}

UHoudiniAssetComponent *
AHoudiniAssetActor::GetHoudiniAssetComponent() const
{
	return HoudiniAssetComponent;
}

/*
#if WITH_EDITOR
bool
AHoudiniAssetActor::ShouldImport(FString * ActorPropString, bool IsMovingLevel)
{
	if (!ActorPropString)
		return false;

	// Locate actor which is being copied in clipboard string.
	AHoudiniAssetActor * CopiedActor = FHoudiniEngineUtils::LocateClipboardActor(this, *ActorPropString);

	// We no longer need clipboard string and can empty it. This seems to avoid occasional crash bug in UE4 which
	// happens on copy / paste.
	ActorPropString->Empty();

	if (!IsValid(CopiedActor))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to import from copy: Duplicated actor not found"));
		return false;
	}

	// Get Houdini component of an actor which is being copied.
	UHoudiniAssetComponent * CopiedActorHoudiniAssetComponent = CopiedActor->HoudiniAssetComponent;
	if (!IsValid(CopiedActorHoudiniAssetComponent))
		return false;

	HoudiniAssetComponent->OnComponentClipboardCopy(CopiedActorHoudiniAssetComponent);

	// If actor is copied through moving, we need to copy main transform.
	const FTransform & ComponentWorldTransform = CopiedActorHoudiniAssetComponent->GetComponentTransform();
	HoudiniAssetComponent->SetWorldLocationAndRotation(
		ComponentWorldTransform.GetLocation(),
		ComponentWorldTransform.GetRotation());

	// We also need to copy actor label.
	const FString & CopiedActorLabel = CopiedActor->GetActorLabel();
	FActorLabelUtilities::SetActorLabelUnique(this, CopiedActorLabel);

	return true;
}
#endif
*/

#if WITH_EDITOR
bool
AHoudiniAssetActor::GetReferencedContentObjects(TArray< UObject * >& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (IsValid(HoudiniAssetComponent))
	{
		UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		if (IsValid(HoudiniAsset))
			Objects.AddUnique(HoudiniAsset);
	}

	return true;
}
#endif

#if WITH_EDITOR
void
AHoudiniAssetActor::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Some property changes need to be forwarded to the component (ie Transform)
	if (!IsValid(HoudiniAssetComponent))
		return;

	FProperty* Property = PropertyChangedEvent.MemberProperty;
	if (!Property)
		return;

	FName PropertyName = Property->GetFName();
	if (PropertyName == HoudiniAssetComponent->GetRelativeLocationPropertyName()
		|| PropertyName == HoudiniAssetComponent->GetRelativeRotationPropertyName()
		|| PropertyName == HoudiniAssetComponent->GetRelativeScale3DPropertyName())
	{
		HoudiniAssetComponent->SetHasComponentTransformChanged(true);
	}
}
#endif


bool
AHoudiniAssetActor::IsUsedForPreview() const
{
#if WITH_EDITORONLY_DATA
	return HasAnyFlags(RF_Transient) || bIsEditorPreviewActor;
#else
	return HasAnyFlags(RF_Transient);
#endif
}

UHoudiniPDGAssetLink*
AHoudiniAssetActor::GetPDGAssetLink() const
{
	return IsValid(HoudiniAssetComponent) ? HoudiniAssetComponent->GetPDGAssetLink() : nullptr;
}

bool AHoudiniAssetActor::IsEditorOnly() const
{
	// In partitioned world, we treat these actors as editor only. Only the baked actors are used for Game and PIE.
	ULevel* Level = GetLevel();
	if (Level && Level->bIsPartitioned)
	{
		return true;
	}
	return Super::IsEditorOnly();
}

#undef LOCTEXT_NAMESPACE
