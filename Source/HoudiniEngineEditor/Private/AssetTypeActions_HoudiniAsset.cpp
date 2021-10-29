/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "AssetTypeActions_HoudiniAsset.h"
#include "HoudiniAsset.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniTool.h"
#include "HoudiniEngineEditorUtils.h"

#include "EditorReimportHandler.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "EditorFramework/AssetImportData.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FText
FAssetTypeActions_HoudiniAsset::GetName() const
{
	return LOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset");
}

FColor
FAssetTypeActions_HoudiniAsset::GetTypeColor() const
{
	return FColor(255, 165, 0);
}

UClass *
FAssetTypeActions_HoudiniAsset::GetSupportedClass() const
{
	return UHoudiniAsset::StaticClass();
}

uint32
FAssetTypeActions_HoudiniAsset::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

/*
UThumbnailInfo *
FAssetTypeActions_HoudiniAsset::GetThumbnailInfo(UObject * Asset) const
{
	if (!IsValid(Asset))
		return nullptr;

	UHoudiniAsset * HoudiniAsset = CastChecked< UHoudiniAsset >(Asset);
	UThumbnailInfo * ThumbnailInfo = HoudiniAsset->ThumbnailInfo;
	if (!ThumbnailInfo)
	{
		// If we have no thumbnail information, construct it.
		ThumbnailInfo = NewObject< USceneThumbnailInfo >(HoudiniAsset, USceneThumbnailInfo::StaticClass());
		HoudiniAsset->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}
*/

bool
FAssetTypeActions_HoudiniAsset::HasActions(const TArray< UObject * > & InObjects) const
{
	return true;
}

void
FAssetTypeActions_HoudiniAsset::GetActions(const TArray<UObject *> & InObjects, class FMenuBuilder & MenuBuilder)
{
	bool ValidObjects = false;
	TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets;
	if (InObjects.Num() > 0)
	{
		HoudiniAssets = GetTypedWeakObjectPtrs<UHoudiniAsset>(InObjects);
		ValidObjects = true;
	}

	FName StyleSetName = FHoudiniEngineStyle::GetStyleSetName();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Reimport"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Reimport selected Houdini Assets."),
		FSlateIcon(StyleSetName, "HoudiniEngine._Reset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteReimport, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_RebuildAll", "Rebuild All Instances"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_RebuildAllTooltip", "Reimports and rebuild all instances of the selected Houdini Assets."),
		FSlateIcon(StyleSetName, "HoudiniEngine._RebuildAll"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteRebuildAllInstances, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorer", "Find Source"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorerTooltip",
			"Opens explorer at the location of this asset."),
		FSlateIcon(StyleSetName, "HoudiniEngine.Hou_OpenInHoudinidiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteFindInExplorer, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_OpenInHoudini", "Open in Houdini"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_OpenInHoudiniTooltip",
			"Opens the selected asset in Houdini."),
		FSlateIcon(StyleSetName, "HoudiniEngine._OpenInHoudini"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteOpenInHoudini, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Instantiate", "Instantiate"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_InstantiateTooltip",
			"Instantiate the selected asset in the current world."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteInstantiate, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_InstantiateOrigin", "Instantiate at the origin"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_InstantiateOriginTooltip",
			"Instantiate the selected asset in the current world. The Houdini Asset Actor will be created at the origin of the level (0,0,0)."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteInstantiateOrigin, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ApplyOpSingle", "Apply to the current selection (single input)"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_ApplyOpSingleTooltip",
			"Applies the selected asset to the current world selection. All the selected object will be assigned to the first input."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteApplyOpSingle, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ApplyOpMulti", "Apply to the current selection (multiple inputs )"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_ApplyOpMultiTooltip",
			"Applies the selected asset to the current world selection. Each selected object will be assigned to its own input (one object per input)."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteApplyOpMulti, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ApplyBatch", "Batch Apply to the current selection"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_ApplyBatchTooltip",
			"Batch apply the selected asset to the current world selection. An instance of the selected Houdini asset will be created for each selected object."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteApplyBatch, HoudiniAssets),
			FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
		)
	);
}


bool 
FAssetTypeActions_HoudiniAsset::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked)
	{
		bool ValidObjects = false;
		TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets;
		if (InObjects.Num() > 0)
		{
			HoudiniAssets = GetTypedWeakObjectPtrs<UHoudiniAsset>(InObjects);
			ValidObjects = true;
		}

		if (ValidObjects)
		{
			ExecuteInstantiate(HoudiniAssets);
			return true;
		}
	}

	return false;
}


TSharedRef<FExtender>
FAssetTypeActions_HoudiniAsset::AddLevelEditorMenuExtenders(TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets)
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();

	FName StyleSetName = FHoudiniEngineStyle::GetStyleSetName();

	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);
	Extender->AddMenuExtension(
		"ActorAsset",
		EExtensionHook::After,
		LevelEditorCommandBindings,
		FMenuExtensionDelegate::CreateLambda([this, HoudiniAssets, StyleSetName](FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_FindInExplorer", "Find Source HDA"),
			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_FindInExplorerTooltip", "Opens an explorer at the location of this actor's source HDA file."),
			FSlateIcon(StyleSetName, "HoudiniEngine.DigitalAsset"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteFindInExplorer, HoudiniAssets),
				FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() > 0); })
			)
		);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_OpenInHoudini", "Open HDA in Houdini"),
			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_OpenInHoudiniTooltip", "Opens the selected asset's source HDA file in Houdini."),
			FSlateIcon(StyleSetName, "HoudiniEngine._OpenInHoudini"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniAsset::ExecuteOpenInHoudini, HoudiniAssets),
				FCanExecuteAction::CreateLambda([=] { return (HoudiniAssets.Num() == 1); })
			)
		);
	})
	);

	return Extender;
}

void
FAssetTypeActions_HoudiniAsset::ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	for (auto ObjIt = InHoudiniAssetPtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniAsset * HoudiniAsset = (*ObjIt).Get();
		if (HoudiniAsset)
			FReimportManager::Instance()->Reimport(HoudiniAsset, true);
	}
}

void
FAssetTypeActions_HoudiniAsset::ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	for (auto ObjIt = InHoudiniAssetPtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniAsset * HoudiniAsset = (*ObjIt).Get();
		if (HoudiniAsset && HoudiniAsset->AssetImportData)
		{
			const FString SourceFilePath = HoudiniAsset->AssetImportData->GetFirstFilename();
			if (SourceFilePath.Len() && IFileManager::Get().FileSize(*SourceFilePath) != INDEX_NONE)
				return FPlatformProcess::ExploreFolder(*SourceFilePath);
		}
	}
}

void
FAssetTypeActions_HoudiniAsset::ExecuteOpenInHoudini(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	if (!FHoudiniEngine::IsInitialized())
		return;

	if (InHoudiniAssetPtrs.Num() != 1)
		return;

	UHoudiniAsset * HoudiniAsset = InHoudiniAssetPtrs[0].Get();
	if (!HoudiniAsset || !(HoudiniAsset->AssetImportData))
		return;

	FString SourceFilePath = HoudiniAsset->AssetImportData->GetFirstFilename();
	if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
		return;

	if (!FPaths::FileExists(SourceFilePath))
		return;

	// We'll need to modify the file name for expanded .hda
	FString FileExtension = FPaths::GetExtension(SourceFilePath);
	if (FileExtension.Compare(TEXT("hdalibrary"), ESearchCase::IgnoreCase) == 0)
	{
		// the .hda directory is what we're actually interested in loading
		SourceFilePath = FPaths::GetPath(SourceFilePath);
	}

	if (FPaths::IsRelative(SourceFilePath))
		FPaths::ConvertRelativePathToFull(SourceFilePath);

	// Then open the HDA file in Houdini
	FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
	FString HoudiniLocation = LibHAPILocation + "/hview";

	FPlatformProcess::CreateProc(
		HoudiniLocation.GetCharArray().GetData(),
		SourceFilePath.GetCharArray().GetData(),
		true, false, false,
		nullptr, 0,
		FPlatformProcess::UserTempDir(),
		nullptr, nullptr);
}


void
FAssetTypeActions_HoudiniAsset::ExecuteRebuildAllInstances(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	// Reimports and then rebuild all instances of the asset
	for (auto ObjIt = InHoudiniAssetPtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniAsset * HoudiniAsset = (*ObjIt).Get();
		if (!HoudiniAsset)
			continue;

		// Reimports the asset
		FReimportManager::Instance()->Reimport(HoudiniAsset, true);

		// Rebuilds all instances of that asset in the scene
		for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
		{
			UHoudiniAssetComponent * Component = *Itr;
			if (Component && (Component->GetHoudiniAsset() == HoudiniAsset))
			{
				Component->MarkAsNeedRebuild();
			}
		}
	}
}


void
FAssetTypeActions_HoudiniAsset::ExecuteApplyOpSingle(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE);
}

void
FAssetTypeActions_HoudiniAsset::ExecuteApplyOpMulti(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI);
}
void
FAssetTypeActions_HoudiniAsset::ExecuteApplyBatch(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH);
}

void
FAssetTypeActions_HoudiniAsset::ExecuteApplyAssetToSelection(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs, const EHoudiniToolType& InType)
{
	if (InHoudiniAssetPtrs.Num() != 1)
		return;

	UHoudiniAsset * HoudiniAsset = InHoudiniAssetPtrs[0].Get();
	if (!HoudiniAsset || !(HoudiniAsset->AssetImportData))
		return;


	FHoudiniEngineEditorUtils::InstantiateHoudiniAsset(HoudiniAsset, InType, EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY);
	/*
	// Creating a temporary tool for the selected asset
	TSoftObjectPtr<UHoudiniAsset> HoudiniAssetPtr(HoudiniAsset);
	FHoudiniTool HoudiniTool(
		HoudiniAssetPtr,
		FText::FromString(HoudiniAsset->GetName()),
		Type,
		EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY,
		FText(),
		NULL,
		FString(),
		false,
		FFilePath(),
		FHoudiniToolDirectory(),
		FString());

	SHoudiniToolPalette::InstantiateHoudiniTool(&HoudiniTool);
	*/
}

void
FAssetTypeActions_HoudiniAsset::ExecuteInstantiateOrigin(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	for (auto HoudiniAssetPtr : InHoudiniAssetPtrs)
	{
		UHoudiniAsset * HoudiniAsset = HoudiniAssetPtr.Get();
		if (!HoudiniAsset || !(HoudiniAsset->AssetImportData))
			continue;

		FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(HoudiniAsset, FTransform::Identity);
	}	
}

void
FAssetTypeActions_HoudiniAsset::ExecuteInstantiate(TArray<TWeakObjectPtr<UHoudiniAsset>> InHoudiniAssetPtrs)
{
	FTransform DefaultTransform = FHoudiniEngineEditorUtils::GetDefaulAssetSpawnTransform();
	for (auto HoudiniAssetPtr : InHoudiniAssetPtrs)
	{
		UHoudiniAsset * HoudiniAsset = HoudiniAssetPtr.Get();
		if (!HoudiniAsset || !(HoudiniAsset->AssetImportData))
			continue;

		FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(HoudiniAsset, DefaultTransform);
	}
}

#undef LOCTEXT_NAMESPACE