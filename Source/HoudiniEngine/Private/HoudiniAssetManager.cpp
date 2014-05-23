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


UHoudiniAssetManager::UHoudiniAssetManager(const FPostConstructInitializeProperties& PCIP) :
Super(PCIP)
{

}


void 
UHoudiniAssetManager::NotifyAssetCreated(UHoudiniAsset* HoudiniAsset)
{
	HoudiniAssets.Add(HoudiniAsset);
}


void 
UHoudiniAssetManager::NotifyAssetDestroyed(UHoudiniAsset* HoudiniAsset)
{
	HoudiniAssets.RemoveSingleSwap(HoudiniAsset);
}


void 
UHoudiniAssetManager::ScheduleAsynchronousCooking(UHoudiniAsset* HoudiniAsset)
{
	if(HoudiniAsset && !HoudiniAsset->IsPendingKill())
	{
		// Mark asset as cooking.
		HoudiniAsset->SetCooking(true);
		
		// Start asynchronous task to perform the cooking from the referenced asset.
		FHoudiniTaskCookAsset* HoudiniTaskCookAsset = new FHoudiniTaskCookAsset(this, HoudiniAsset);

		// Create a new thread to execute our runnable.
		FRunnableThread* Thread = FRunnableThread::Create(HoudiniTaskCookAsset, TEXT("HoudiniTaskCookAsset"), true, true, 0, TPri_Normal);
	}
}


void 
UHoudiniAssetManager::NotifyAssetCookingFailed(UHoudiniAsset* HoudiniAsset, HAPI_Result Result)
{
	// Mark asset as not cooking.
	HoudiniAsset->SetCooking(false);

	HOUDINI_LOG_ERROR(TEXT("HoudiniAssetManager: failed cooking asset: 0x%0.8p, error: %s"), HoudiniAsset, *FHoudiniEngineUtils::GetErrorDescription(Result));
}


void 
UHoudiniAssetManager::NotifyAssetCookingFinished(UHoudiniAsset* HoudiniAsset, HAPI_AssetId AssetId, const std::string& AssetInternalName)
{
	// Mark asset as not cooking.
	HoudiniAsset->SetCooking(false);
	
	// Mark asset as cooked.
	HoudiniAsset->SetCooked(true);

	HoudiniAsset->AssetInternalName = AssetInternalName;
	HoudiniAsset->AssetId = AssetId;	
}
