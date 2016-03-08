/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"


AHoudiniAssetActor::AHoudiniAssetActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	CurrentPlayTime(0.0f)
{
	bCanBeDamaged = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create Houdini component and attach it to a root component.
	HoudiniAssetComponent = ObjectInitializer.CreateDefaultSubobject<UHoudiniAssetComponent>(this,
		TEXT("HoudiniAssetComponent"));

	HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RootComponent = HoudiniAssetComponent;
}


UHoudiniAssetComponent*
AHoudiniAssetActor::GetHoudiniAssetComponent() const
{
	return HoudiniAssetComponent;
}


bool
AHoudiniAssetActor::IsUsedForPreview() const
{
	return HasAnyFlags(RF_Transient);
}


void
AHoudiniAssetActor::ResetHoudiniCurrentPlaytime()
{
	CurrentPlayTime = 0.0f;
}


float
AHoudiniAssetActor::GetHoudiniCurrentPlaytime() const
{
	return CurrentPlayTime;
}


void
AHoudiniAssetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR

	if(HoudiniAssetComponent->bTimeCookInPlaymode)
	{
		HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();
		if(-1 == AssetId)
		{
			// If component is not instantiating or cooking, we can set time and force cook.
			if(!HoudiniAssetComponent->IsInstantiatingOrCooking())
			{
				FHoudiniEngineUtils::SetCurrentTime(0.0f);
				HoudiniAssetComponent->StartTaskAssetCookingManual();
			}
		}
		else
		{
			FHoudiniEngineUtils::SetCurrentTime(CurrentPlayTime);
			FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr);

			HoudiniAssetComponent->PostCook();
		}

		// Increment play time.
		CurrentPlayTime += DeltaSeconds;
	}

#endif

}


#if WITH_EDITOR

bool
AHoudiniAssetActor::ShouldImport(FString* ActorPropString, bool IsMovingLevel)
{
	if(ActorPropString)
	{
		// Locate actor which is being copied in clipboard string.
		AHoudiniAssetActor* CopiedActor = FHoudiniEngineUtils::LocateClipboardActor(*ActorPropString);

		// We no longer need clipboard string and can empty it. This seems to avoid occasional crash bug in UE4 which
		// happens on copy / paste.
		ActorPropString->Empty();

		if(CopiedActor)
		{
			// Get Houdini component of an actor which is being copied.
			UHoudiniAssetComponent* CopiedActorHoudiniAssetComponent = CopiedActor->HoudiniAssetComponent;
			if(CopiedActorHoudiniAssetComponent)
			{
				HoudiniAssetComponent->OnComponentClipboardCopy(CopiedActorHoudiniAssetComponent);

				// If actor is copied through moving, we need to copy main transform.
				const FTransform& ComponentWorldTransform = CopiedActorHoudiniAssetComponent->GetComponentTransform();
					HoudiniAssetComponent->SetWorldLocationAndRotation(ComponentWorldTransform.GetLocation(),
						ComponentWorldTransform.GetRotation());

				// We also need to copy actor label.
				const FString& CopiedActorLabel = CopiedActor->GetActorLabel();
				FActorLabelUtilities::SetActorLabelUnique(this, CopiedActorLabel);

				return true;
			}
		}
	}

	return false;
}


bool
AHoudiniAssetActor::ShouldExport()
{
	return true;
}

#endif
