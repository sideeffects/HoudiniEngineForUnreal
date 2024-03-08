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

#pragma once

#include "CoreMinimal.h"
#include "AssetToolsModule.h"
#include "HoudiniToolsPackageAsset.h"
#include "Factories/Factory.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"
#include "HoudiniToolsEditor.h"
#include "HoudiniToolsRuntimeUtils.h"

#include "HoudiniToolsPackageAssetFactory.generated.h"

enum class EAssetCommandResult : uint8;
struct FAssetCategoryPath;
struct FAssetOpenArgs;


// --------------------------------------------
// HoudiniToolsPackageAsset factory
// --------------------------------------------

UCLASS(hidecategories = Object)
class UHoudiniToolsPackageAssetFactory : public UFactory
{
	GENERATED_BODY()

public:

	UHoudiniToolsPackageAssetFactory()
	{
		SupportedClass = UHoudiniToolsPackageAsset::StaticClass();

		bCreateNew = true;
		bEditAfterNew = true;
	}

	virtual FText GetDisplayName() const override
	{
		return NSLOCTEXT("DisplayName", "AssetDefinition_HoudiniToolsPackage", "Houdini Tools Package");
	}

	virtual FText GetToolTip() const override
	{
		return NSLOCTEXT("Tooltip", "AssetDefinition_HoudiniToolsPackage", "Describes a Houdini Tools package.");
	}

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		UHoudiniToolsPackageAsset* ToolsPackage = nullptr;
		if (ensure(SupportedClass == Class))
		{
			ensure(0 != (RF_Public & Flags));
			ToolsPackage = NewObject<UHoudiniToolsPackageAsset>(InParent, Class, Name, Flags);
			FHoudiniToolsEditor::PopulatePackageWithDefaultData(ToolsPackage);
		}

		// If the name is not "HoudiniToolsPackage", rename it immediately
		FString DefaultName = FHoudiniToolsRuntimeUtils::GetPackageUAssetName();
		if (Name.ToString() != DefaultName)
		{
			if (!FHoudiniToolsRuntimeUtils::ShowToolsPackageRenameConfirmDialog())
			{
				ToolsPackage->Rename(*DefaultName);
			}
			else
			{
				// Dont rename back, but add a warning in the logs...
				HOUDINI_LOG_WARNING(TEXT("Renaming a HoudiniToolsPackage to anything but \"HoudiniToolsPackage\" disables it."));
			}
		}


		return ToolsPackage;
	}

	virtual uint32 GetMenuCategories() const override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return AssetTools.RegisterAdvancedAssetCategory("HoudiniEngine", NSLOCTEXT("AssetCategoryName", "HoudiniEngineCategory", "Houdini Engine"));
	}

	virtual FString GetDefaultNewAssetName() const override
	{
		return TEXT("HoudiniToolsPackage");
	}

	virtual bool ShouldShowInNewMenu() const override { return true; }
};


// --------------------------------------------
// HoudiniToolsPackage asset definition
// --------------------------------------------
//
// UCLASS()
// class UAssetDefinition_HoudiniToolsPackageAsset : public UAssetDefinitionDefault
// {
// 	GENERATED_BODY()
//
// public:
// 	// UAssetDefinition Begin
// 	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_HoudiniToolsPackageAsset", "Houdini Tools Package Asset"); }
// 	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(175, 0, 128)); }
// 	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UHoudiniToolsPackageAsset::StaticClass(); }
// 	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
// 	{
// 		return TConstArrayView<FAssetCategoryPath>();
// 	}
// 	virtual FText GetObjectDisplayNameText(UObject* Object) const override { return FText::FromString(TEXT("UHoudiniToolsPackageAsset")); }
// 	// UAssetDefinition End
// };	
