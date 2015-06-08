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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"


UHoudiniAssetActorFactory::UHoudiniAssetActorFactory(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("HoudiniAssetDisplayName", "Houdini Engine Asset");
	NewActorClass = AHoudiniAssetActor::StaticClass();
}


bool
UHoudiniAssetActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if(!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UHoudiniAsset::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoHoudiniAsset", "A valid Houdini Engine asset must be specified.");
		return false;
	}

	return true;
}


UObject*
UHoudiniAssetActorFactory::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(Instance);

	check(HoudiniAssetActor->HoudiniAssetComponent);
	return HoudiniAssetActor->GetHoudiniAssetComponent()->HoudiniAsset;
}


void
UHoudiniAssetActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostSpawnActor, supplied Asset = 0x%0.8p"), Asset);

	UHoudiniAsset* HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
	if(HoudiniAsset)
	{
		AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(NewActor);
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		check(HoudiniAssetComponent);

		// Mark this component as native.
		HoudiniAssetComponent->SetNative(true);

		HoudiniAssetComponent->UnregisterComponent();
		HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
		HoudiniAssetComponent->RegisterComponent();
	}
}


void
UHoudiniAssetActorFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostCreateBlueprint, supplied Asset = 0x%0.8p"), Asset);

	UHoudiniAsset* HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
	if(HoudiniAsset)
	{
		AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(CDO);
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		HoudiniAssetComponent->HoudiniAsset = HoudiniAsset;
	}
}
