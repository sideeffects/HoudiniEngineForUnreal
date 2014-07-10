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

#include "HoudiniEnginePrivatePCH.h"


AHoudiniAssetActor::AHoudiniAssetActor(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	bWantsInitialize = false;
	bCanBeDamaged = false;

	// Create our root component.
	TSubobjectPtr<USceneComponent> HoudiniRootComponent = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("HoudiniRootComponent"));
	RootComponent = HoudiniRootComponent;

	/*
	// Create Houdini component and attach it to a root component.
	HoudiniAssetComponent = PCIP.CreateDefaultSubobject<UHoudiniAssetComponent>(this, TEXT("HoudiniAssetComponent"));
	HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HoudiniAssetComponent->AttachParent = RootComponent;
	*/

	HoudiniAssetComponent = ConstructObject<UHoudiniAssetComponent>(UHoudiniAssetComponent::StaticClass(), this);
	HoudiniAssetComponent->HoudiniAssetActorOwner = this;
	HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HoudiniAssetComponent->AttachParent = RootComponent;

	HoudiniAssetComponents.Add(HoudiniAssetComponent);
}


bool
AHoudiniAssetActor::IsUsedForPreview() const
{
	return HasAnyFlags(RF_Transient);
}


bool
AHoudiniAssetActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	//if(HoudiniAssetComponent.IsValid())
	if(HoudiniAssetComponent)
	{
		// Retrieve the asset associated with this component.
		UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		if(HoudiniAsset)
		{
			Objects.Add(HoudiniAsset);
		}
	}

	return true;
}


/*
void
AHoudiniAssetActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	AHoudiniAssetActor* ThisActor = CastChecked<AHoudiniAssetActor>(InThis);
	if(ThisActor && !ThisActor->IsPendingKill() && ThisActor->HoudiniAssetComponent.IsValid())
	{
		// Retrieve the asset associated with this component.
		UHoudiniAsset* HoudiniAsset = ThisActor->HoudiniAssetComponent->GetHoudiniAsset();
		if(HoudiniAsset)
		{
			Collector.AddReferencedObject(HoudiniAsset, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}
*/
