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


//--
FText 
FHoudiniEngineAssetTypeActions::GetName() const
{
	return LOCTEXT("FHoudiniEngineAssetTypeActions", "HoudiniEngineAsset");
}


//--
FColor 
FHoudiniEngineAssetTypeActions::GetTypeColor() const
{
	//orange
	return FColor(0, 255, 255);
}


//--
UClass* 
FHoudiniEngineAssetTypeActions::GetSupportedClass() const
{
	return UHoudiniEngineAsset::StaticClass();
}


//--
bool 
FHoudiniEngineAssetTypeActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}


//--
void 
FHoudiniEngineAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto HoudiniEngineAssets = GetTypedWeakObjectPtrs<UHoudiniEngineAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniEngineAssetTypeActions", "HoudiniEngineAsset_Edit", "Edit"),
		NSLOCTEXT("HoudiniEngineAssetTypeActions", "HoudiniEngineAsset_EditTooltip", "Opens the selected Houdini Engine asset in the editor."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FHoudiniEngineAssetTypeActions::ExecuteEdit, HoudiniEngineAssets),
		FCanExecuteAction()
		)
		);

	/*
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniEngineAssetTypeActions", "HoudiniEngineAsset_FindInExplorer", "Find Source"),
		NSLOCTEXT("HoudiniEngineAssetTypeActions", "HoudiniEngineAsset_FindInExplorerTooltip", "Opens explorer at the location of this asset."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FHoudiniEngineAssetTypeActions::ExecuteFindInExplorer, HoudiniEngineAssets),
		FCanExecuteAction::CreateSP(this, &FHoudiniEngineAssetTypeActions::CanExecuteSourceCommands, HoudiniEngineAssets)
		)
		);
	*/
}


//--
void 
FHoudiniEngineAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniEngineAsset* HoudiniEngineAsset = Cast<UHoudiniEngineAsset>(*ObjIt);
		if (HoudiniEngineAsset)
		{
			IHoudiniEngineEditorModule* HoudiniEngineEditorModule = &FModuleManager::LoadModuleChecked<IHoudiniEngineEditorModule>("HoudiniEngineEditor");
			HoudiniEngineEditorModule->CreateHoudiniEngineEditor(Mode, EditWithinLevelEditor, HoudiniEngineAsset);
		}
	}
}


//--
uint32 
FHoudiniEngineAssetTypeActions::GetCategories()
{
	//return EAssetTypeCategories::Misc;
	return EAssetTypeCategories::MaterialsAndTextures;
}


//--
void 
FHoudiniEngineAssetTypeActions::ExecuteEdit(TArray<TWeakObjectPtr<UHoudiniEngineAsset> > Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Object);
		}
	}
}


//--
void 
FHoudiniEngineAssetTypeActions::ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniEngineAsset> > Objects)
{
	/*
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object && Object->AssetImportData)
		{
			const FString SourceFilePath = FReimportManager::ResolveImportFilename(Object->AssetImportData->SourceFilePath, Object);
			if (SourceFilePath.Len() && IFileManager::Get().FileSize(*SourceFilePath) != INDEX_NONE)
			{
				FPlatformProcess::ExploreFolder(*FPaths::GetPath(SourceFilePath));
			}
		}
	}
	*/
}
