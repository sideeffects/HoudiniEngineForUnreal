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
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"


FHoudiniAssetBroker::~FHoudiniAssetBroker()
{

}


UClass*
FHoudiniAssetBroker::GetSupportedAssetClass()
{
	return UHoudiniAsset::StaticClass();
}


bool
FHoudiniAssetBroker::AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset)
{
	if(UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InComponent))
	{
		UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(InAsset);

		if(HoudiniAsset || !InAsset)
		{
			HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
			return true;
		}
	}

	return false;
}


UObject*
FHoudiniAssetBroker::GetAssetFromComponent(UActorComponent* InComponent)
{
	if(UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InComponent))
	{
		return HoudiniAssetComponent->GetHoudiniAsset();
	}

	return nullptr;
}
