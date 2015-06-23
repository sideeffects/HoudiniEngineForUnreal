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


AHoudiniAssetActor::AHoudiniAssetActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	CurrentPlayTime(0.0f)
{
	bCanBeDamaged = false;
	PrimaryActorTick.bCanEverTick = true;

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

	// Set current time through HAPI.

	// Increment play time.
	CurrentPlayTime += DeltaSeconds;
}
