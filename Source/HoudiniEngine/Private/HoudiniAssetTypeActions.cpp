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


bool
FHoudiniAssetTypeActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}


void
FHoudiniAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder)
{
	auto HoudiniAssets = GetTypedWeakObjectPtrs<UHoudiniAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Reimport"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Reimport selected Houdini Assets."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniAssetTypeActions::ExecuteReimport, HoudiniAssets),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorer", "Find Source"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorerTooltip", "Opens explorer at the location of this asset."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FHoudiniAssetTypeActions::ExecuteFindInExplorer, HoudiniAssets),
			FCanExecuteAction()
		)
	);
}


void
FHoudiniAssetTypeActions::ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets)
{
	for(auto ObjIt = HoudiniAssets.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniAsset* Object = (*ObjIt).Get();
		if(Object)
		{
			//FReimportManager::Instance()->Reimport(Object, true);
		}
	}
}


void
FHoudiniAssetTypeActions::ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets)
{
	for(auto ObjIt = HoudiniAssets.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniAsset* Object = (*ObjIt).Get();
		if(Object)
		{
			const FString SourceFilePath = FReimportManager::ResolveImportFilename(Object->OTLFileName, Object);
			if(INDEX_NONE != SourceFilePath.Len() && IFileManager::Get().FileSize(*SourceFilePath))
			{
				FPlatformProcess::ExploreFolder(*SourceFilePath);
			}
		}
	}
}
