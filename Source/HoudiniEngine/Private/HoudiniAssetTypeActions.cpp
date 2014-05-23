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


FText
FHoudiniAssetTypeActions::GetName() const
{
	return LOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset");
}


FColor
FHoudiniAssetTypeActions::GetTypeColor() const
{
	return FColor(255, 165, 0);
}


UClass*
FHoudiniAssetTypeActions::GetSupportedClass() const
{
	return UHoudiniAsset::StaticClass();
}


uint32
FHoudiniAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}


UThumbnailInfo* 
FHoudiniAssetTypeActions::GetThumbnailInfo(UObject* Asset) const
{
	UHoudiniAsset* HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = HoudiniAsset->ThumbnailInfo;
	if(!ThumbnailInfo)
	{
		// If we have no thumbnail information, construct it.
		ThumbnailInfo = ConstructObject<USceneThumbnailInfo>(USceneThumbnailInfo::StaticClass(), HoudiniAsset);
		HoudiniAsset->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}
