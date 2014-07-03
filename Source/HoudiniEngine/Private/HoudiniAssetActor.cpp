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

	HoudiniAssetComponent = PCIP.CreateDefaultSubobject<UHoudiniAssetComponent>(this, TEXT("HoudiniAssetComponent"));
	HoudiniAssetComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// Our component is a root component.
	RootComponent = HoudiniAssetComponent;
}


bool
AHoudiniAssetActor::IsUsedForPreview() const
{
	return HasAnyFlags(RF_Transient);
}
