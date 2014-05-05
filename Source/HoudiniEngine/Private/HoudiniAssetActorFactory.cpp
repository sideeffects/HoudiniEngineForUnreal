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

#include "HoudiniEnginePrivatePCH.h"


UHoudiniAssetActorFactory::UHoudiniAssetActorFactory(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP)
{
	DisplayName = LOCTEXT("HoudiniAssetDisplayName", "Houdini Engine Asset");
	NewActorClass = AHoudiniAssetActor::StaticClass();
}


bool 
UHoudiniAssetActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UHoudiniAsset::StaticClass()))
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
	return HoudiniAssetActor->HoudiniAssetComponent->HoudiniAsset;
}


void 
UHoudiniAssetActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	UHoudiniAsset* HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
	GEditor->SetActorLabelUnique(NewActor, HoudiniAsset->GetName());

	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(NewActor);
	UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->HoudiniAssetComponent;
	check(HoudiniAssetComponent);

	HoudiniAssetComponent->UnregisterComponent();

	HoudiniAssetComponent->HoudiniAsset = HoudiniAsset;
	//HoudiniAssetComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;

	HoudiniAssetComponent->RegisterComponent();
}


void 
UHoudiniAssetActorFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	UHoudiniAsset* HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(CDO);
	UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->HoudiniAssetComponent;

	HoudiniAssetComponent->HoudiniAsset = HoudiniAsset;
	//StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
}
