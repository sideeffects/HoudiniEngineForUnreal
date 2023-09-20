/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "AssetTypeActions_HoudiniToolsPackageAsset.h"
#include "HoudiniToolsPackageAsset.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"

#include "HoudiniEngineRuntimeUtils.h"

#include "HoudiniToolsEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FText
FAssetTypeActions_HoudiniToolsPackageAsset::GetName() const
{
	return LOCTEXT("HoudiniAssetTypeActions", "HoudiniLibrary");
}

FColor
FAssetTypeActions_HoudiniToolsPackageAsset::GetTypeColor() const
{
	return FColor(255, 165, 0);
}

UClass *
FAssetTypeActions_HoudiniToolsPackageAsset::GetSupportedClass() const
{
	return UHoudiniToolsPackageAsset::StaticClass();
}

uint32
FAssetTypeActions_HoudiniToolsPackageAsset::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool
FAssetTypeActions_HoudiniToolsPackageAsset::HasActions(const TArray< UObject * > & InObjects) const
{
	return true;
}

void
FAssetTypeActions_HoudiniToolsPackageAsset::GetActions(const TArray<UObject *> & InObjects, class FMenuBuilder & MenuBuilder)
{
	bool ValidObjects = false;
	TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> HoudiniPackages;
	if (InObjects.Num() > 0)
	{
		HoudiniPackages = GetTypedWeakObjectPtrs<UHoudiniToolsPackageAsset>(InObjects);
		ValidObjects = true;
	}

	FName StyleSetName = FHoudiniEngineStyle::GetStyleSetName();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Reimport"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Reimport package description and HDAs, if permitted by package settings."),
		FSlateIcon(StyleSetName, "HoudiniEngine._Reset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteReimport, HoudiniPackages),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Reimport package description"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Reimport package description only, if permitted by package settings."),
		FSlateIcon(StyleSetName, "HoudiniEngine._Reset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteReimportDescriptionOnly, HoudiniPackages),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Find new HDAs"),
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Find and import new HDAs into this package."),
		FSlateIcon(StyleSetName, "HoudiniEngine._Reset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteImportNewHDAs, HoudiniPackages),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorer", "Find Source Package"),
		NSLOCTEXT(
			"HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorerTooltip",
			"Opens explorer at the location of this asset."),
		FSlateIcon(StyleSetName, "HoudiniEngine.DigitalAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteFindInExplorer, HoudiniPackages),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);

	
}


// bool 
// FAssetTypeActions_HoudiniToolsPackageAsset::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
// {
// 	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked)
// 	{
// 		bool ValidObjects = false;
// 		TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets;
// 		if (InObjects.Num() > 0)
// 		{
// 			HoudiniAssets = GetTypedWeakObjectPtrs<UHoudiniAsset>(InObjects);
// 			ValidObjects = true;
// 		}
//
// 		if (ValidObjects)
// 		{
// 			ExecuteInstantiate(HoudiniAssets);
// 			return true;
// 		}
// 	}
//
// 	return false;
// }


// TSharedRef<FExtender>
// FAssetTypeActions_HoudiniToolsPackageAsset::AddLevelEditorMenuExtenders(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> PackageAssets)
// {
// 	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
// 	TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();
//
// 	FName StyleSetName = FHoudiniEngineStyle::GetStyleSetName();
//
// 	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);
// 	Extender->AddMenuExtension(
// 		"ActorAsset",
// 		EExtensionHook::After,
// 		LevelEditorCommandBindings,
// 		FMenuExtensionDelegate::CreateLambda([this, PackageAssets, StyleSetName](FMenuBuilder& MenuBuilder)
// 	{
// 		MenuBuilder.AddMenuEntry(
// 			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_FindInExplorer", "Find Source Package"),
// 			NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_FindInExplorerTooltip", "Opens explorer at the source location for this package."),
// 			FSlateIcon(StyleSetName, "HoudiniEngine.DigitalAsset"),
// 			FUIAction(
// 				FExecuteAction::CreateSP(this, &FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteFindInExplorer, PackageAssets),
// 				FCanExecuteAction::CreateLambda([=] { return (PackageAssets.Num() > 0); })
// 			)
// 		);
// 	})
// 	);
//
// 	return Extender;
// }

void
FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> InPackagePtrs)
{
	for (auto ObjIt = InPackagePtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniToolsPackageAsset* ToolsPackage = (*ObjIt).Get();
		if (!IsValid(ToolsPackage))
		{
			continue;
		}
		
		// Attempt to import the package description
		FHoudiniToolsEditor::ReimportExternalToolsPackageDescription(ToolsPackage);
		FHoudiniToolsEditor::ReimportPackageHDAs(ToolsPackage);

		if (GEditor)
		{
			// Since we're doing a manual reimport of external data, we're manually triggering reimport events here
			// to allow the Tools panel to refresh.
			GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetReimport(ToolsPackage);
		}
	}
}

void
FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteReimportDescriptionOnly(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> InPackagePtrs)
{
	for (auto ObjIt = InPackagePtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniToolsPackageAsset* ToolsPackage = (*ObjIt).Get();
		if (!IsValid(ToolsPackage))
		{
			continue;
		}
		
		// Attempt to import the package description
		FHoudiniToolsEditor::ReimportExternalToolsPackageDescription(ToolsPackage);
		if (GEditor)
		{
			// Since we're doing a manual reimport of external data, we're manually triggering reimport events here
			// to allow the Tools panel to refresh.
			GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetReimport(ToolsPackage);
		}
	}
}

void
FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteImportNewHDAs(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> InPackagePtrs)
{
	for (auto ObjIt = InPackagePtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UHoudiniToolsPackageAsset* ToolsPackage = (*ObjIt).Get();
		if (!IsValid(ToolsPackage))
		{
			continue;
		}
		
		// Only import missing HDAs into this package.
		FHoudiniToolsEditor::ReimportPackageHDAs(ToolsPackage, true);
	}
}

void
FAssetTypeActions_HoudiniToolsPackageAsset::ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> InPackagePtrs)
{
	for (auto ObjIt = InPackagePtrs.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		const UHoudiniToolsPackageAsset* ToolsPackage = (*ObjIt).Get();
		if (!IsValid(ToolsPackage))
		{
			continue;
		}

		const FString PackageDir = FHoudiniToolsEditor::GetAbsoluteToolsPackagePath(ToolsPackage);
		if (PackageDir.Len() == 0)
		{
			UE_LOG(LogHoudiniTools, Warning, TEXT("Tools Package (%s) does not have a valid external path."), *ToolsPackage->GetPathName());
			continue;
		}

		if (!FPaths::DirectoryExists(PackageDir))
		{
			UE_LOG(LogHoudiniTools, Warning, TEXT("Tools Package (%s) external path '%s' does not exist."), *ToolsPackage->GetPathName(), *PackageDir);
			continue;
		}

		return FPlatformProcess::ExploreFolder(*PackageDir);
	}
}



#undef LOCTEXT_NAMESPACE