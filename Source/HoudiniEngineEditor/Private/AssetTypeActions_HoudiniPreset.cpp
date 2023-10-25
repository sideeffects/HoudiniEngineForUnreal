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

#include "AssetTypeActions_HoudiniPreset.h"

#include "HoudiniAsset.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniPreset.h"
#include "HoudiniToolsEditor.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FText
FAssetTypeActions_HoudiniPreset::GetName() const
{
	return LOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset");
}

FColor
FAssetTypeActions_HoudiniPreset::GetTypeColor() const
{
	return FHoudiniEngineStyle::GetHoudiniPresetColor().ToFColor(true);
}

UClass *
FAssetTypeActions_HoudiniPreset::GetSupportedClass() const
{
	return UHoudiniPreset::StaticClass();
}

uint32
FAssetTypeActions_HoudiniPreset::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool
FAssetTypeActions_HoudiniPreset::HasActions(const TArray< UObject * > & InObjects) const
{
	return true;
}

void FAssetTypeActions_HoudiniPreset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	bool ValidObjects = false;
	TArray<TWeakObjectPtr<UHoudiniPreset>> PresetAssets;
	if (InObjects.Num() > 0)
	{
		PresetAssets = GetTypedWeakObjectPtrs<UHoudiniPreset>(InObjects);
		ValidObjects = true;
	}

	FName StyleSetName = FHoudiniEngineStyle::GetStyleSetName();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_EditToolProperties", "Edit Tool Properties"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_EditToolProperties_Tooltip",
			"Edit the HoudiniPreset asset properties."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteEditToolProperties, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return ValidObjects; })
		)
	);
	

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_Instantiate", "Instantiate"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_InstantiateTooltip",
			"Instantiate the selected asset in the current world."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteInstantiate, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_InstantiateOrigin", "Instantiate at the origin"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_InstantiateOriginTooltip",
			"Instantiate the selected asset in the current world. The Houdini Asset Actor will be created at the origin of the level (0,0,0)."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteInstantiateOrigin, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_ApplyPresetToSelection", "Apply preset to HoudiniAsset actors"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_ApplyPresetToSelectionTooltip",
			"Applies the preset to the parameters of all the selected HoudiniAsset actors, if allowed by the preset."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteApplyPreset, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_ApplyOpSingle", "Apply to the current selection (single input)"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_ApplyOpSingleTooltip",
			"Applies the selected asset to the current world selection. All the selected object will be assigned to the first input."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteApplyOpSingle, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_ApplyOpMulti", "Apply to the current selection (multiple inputs )"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_ApplyOpMultiTooltip",
			"Applies the selected asset to the current world selection. Each selected object will be assigned to its own input (one object per input)."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteApplyOpMulti, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("HoudiniPresetTypeActions", "HoudiniPreset_ApplyBatch", "Batch Apply to the current selection"),
		NSLOCTEXT(
			"HoudiniPresetTypeActions", "HoudiniPreset_ApplyBatchTooltip",
			"Batch apply the selected asset to the current world selection. An instance of the selected Houdini asset will be created for each selected object."),
		FSlateIcon(StyleSetName, "HoudiniEngine.HoudiniEngineLogo"),
		FUIAction(
			FExecuteAction::CreateStatic(&FAssetTypeActions_HoudiniPreset::ExecuteApplyBatch, PresetAssets),
			FCanExecuteAction::CreateLambda([=] { return (PresetAssets.Num() == 1); })
		)
	);
}

void FAssetTypeActions_HoudiniPreset::ExecuteApplyPreset(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	for(TWeakObjectPtr<UHoudiniPreset> PresetPtr : InHoudiniAssetPtrs)
	{
		if (!PresetPtr.IsValid())
		{
			continue;
		}

		const UHoudiniPreset* HoudiniPreset = PresetPtr.Get();
		FHoudiniToolsEditor::ApplyPresetToSelectedHoudiniAssetActors(HoudiniPreset);
	}
}

void
FAssetTypeActions_HoudiniPreset::ExecuteApplyOpSingle(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE);
}

void
FAssetTypeActions_HoudiniPreset::ExecuteApplyOpMulti(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI);
}
void
FAssetTypeActions_HoudiniPreset::ExecuteApplyBatch(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	return ExecuteApplyAssetToSelection(InHoudiniAssetPtrs, EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH);
}

void FAssetTypeActions_HoudiniPreset::ExecuteEditToolProperties(
	TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	for(TWeakObjectPtr<UHoudiniPreset> PresetPtr : InHoudiniAssetPtrs)
	{
		if (!PresetPtr.IsValid())
		{
			continue;
		}

		const UHoudiniPreset* HoudiniPreset = PresetPtr.Get();
		const UHoudiniAsset* HoudiniAsset = PresetPtr->SourceHoudiniAsset;
		const UHoudiniToolsPackageAsset* ToolsPackage = FHoudiniToolsEditor::FindOwningToolsPackage(HoudiniPreset);

		// Create, and populate, a new FHoudiniTool for this asset which is needed to launch the ToolPropertyEditor.
		TSharedPtr<FHoudiniTool> HoudiniTool;
		HoudiniTool = MakeShareable( new FHoudiniTool() );
		FHoudiniToolsEditor& Tools = FHoudiniEngineEditor::Get().GetHoudiniTools();
		Tools.PopulateHoudiniTool(HoudiniTool, HoudiniAsset, HoudiniPreset, ToolsPackage, false);
		FHoudiniToolsEditor::LaunchHoudiniToolPropertyEditor(HoudiniTool);
	}
}

void
FAssetTypeActions_HoudiniPreset::ExecuteApplyAssetToSelection(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs, const EHoudiniToolType& InType)
{
	if (InHoudiniAssetPtrs.Num() != 1)
		return;

	UHoudiniPreset* HoudiniPreset = InHoudiniAssetPtrs[0].Get();
	if (!IsValid(HoudiniPreset))
	{
		return;	
	}
	
	UHoudiniAsset* HoudiniAsset = HoudiniPreset->SourceHoudiniAsset; 
	if (!IsValid(HoudiniAsset) || !(IsValid(HoudiniAsset->AssetImportData)))
		return;

	FHoudiniEngineEditorUtils::InstantiateHoudiniAsset(HoudiniAsset, InType, EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY, HoudiniPreset);
}

void
FAssetTypeActions_HoudiniPreset::ExecuteInstantiateOrigin(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	for (auto HoudiniAssetPtr : InHoudiniAssetPtrs)
	{
		UHoudiniPreset* HoudiniPreset = HoudiniAssetPtr.Get();
		if (!IsValid(HoudiniPreset))
		{
			continue;
		}
		UHoudiniAsset* HoudiniAsset = HoudiniPreset->SourceHoudiniAsset;
		if (!IsValid(HoudiniAsset) || !IsValid(HoudiniAsset->AssetImportData))
			continue;
		
		FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(HoudiniAsset, FTransform::Identity, nullptr, nullptr, HoudiniPreset);
	}
}

void
FAssetTypeActions_HoudiniPreset::ExecuteInstantiate(TArray<TWeakObjectPtr<UHoudiniPreset>> InHoudiniAssetPtrs)
{
	FTransform DefaultTransform = FHoudiniEngineEditorUtils::GetDefaulAssetSpawnTransform();
	for (auto HoudiniAssetPtr : InHoudiniAssetPtrs)
	{
		UHoudiniPreset* HoudiniPreset = HoudiniAssetPtr.Get();
		if (!IsValid(HoudiniPreset))
		{
			continue;
		}
		
		UHoudiniAsset* HoudiniAsset = HoudiniPreset->SourceHoudiniAsset;
		if (!IsValid(HoudiniAsset) || !IsValid(HoudiniAsset->AssetImportData))
		{
			continue;
		}
		
		FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(HoudiniAsset, DefaultTransform, nullptr, nullptr, HoudiniPreset);
	}
}


#undef LOCTEXT_NAMESPACE
