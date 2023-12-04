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
#include "HoudiniEngineRuntime.h"
#include "HoudiniNodeSyncComponent.h"
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
		ObjectInitializer.CreateDefaultSubobject<UHoudiniAssetComponent>(this, TEXT("HoudiniAssetComponent"));

	//HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	RootComponent = HoudiniAssetComponent;
}


void 
AHoudiniAssetActor::SetNodeSyncActor(bool bNodeSyncActor)
{
	if (IsNodeSyncActor() == bNodeSyncActor)
		return;

	// Destroy the existing component
	HoudiniAssetComponent->DestroyComponent();

	if (bNodeSyncActor)
	{
		// Create a new NodeSyncComponent to replace it
		HoudiniAssetComponent = NewObject<UHoudiniNodeSyncComponent>(this);
		RootComponent = HoudiniAssetComponent;
	
		HoudiniAssetComponent->RegisterComponent();
		//HoudiniAssetComponent->AttachToActor(this);
		AddInstanceComponent(HoudiniAssetComponent);

		FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(HoudiniAssetComponent);
	}
	else
	{
		// Create a new HoudiniAssetComponent to replace it
		HoudiniAssetComponent = NewObject<UHoudiniAssetComponent>(this);
		RootComponent = HoudiniAssetComponent;

		HoudiniAssetComponent->RegisterComponent();
		//HoudiniAssetComponent->AttachToActor(this);
		AddInstanceComponent(HoudiniAssetComponent);

		FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(HoudiniAssetComponent);
	}
}

// Indicates if this Actor is a NodeSyncActor
bool
AHoudiniAssetActor::IsNodeSyncActor() const
{
	return HoudiniAssetComponent->IsA<UHoudiniNodeSyncComponent>();
}


UHoudiniAssetComponent *
AHoudiniAssetActor::GetHoudiniAssetComponent() const
{
	return HoudiniAssetComponent;
}

#if WITH_EDITOR
bool
AHoudiniAssetActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
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

#undef LOCTEXT_NAMESPACE
