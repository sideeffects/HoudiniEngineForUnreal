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


AHoudiniAssetActor::AHoudiniAssetActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bCanBeDamaged = false;

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

