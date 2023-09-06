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
#include "HoudiniPreset.h"
#include "Factories/Factory.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"

#include "HoudiniPresetFactory.generated.h"

enum class EAssetCommandResult : uint8;
struct FAssetCategoryPath;
struct FAssetOpenArgs;


// --------------------------------------------
// HoudiniToolsPackageAsset factory
// --------------------------------------------

UCLASS(hidecategories = Object)
class UHoudiniPresetFactory : public UFactory
{
	GENERATED_BODY()

public:

	UHoudiniPresetFactory()
	{
		SupportedClass = UHoudiniPreset::StaticClass();

		bCreateNew = true;
		bEditAfterNew = true;
	}

	FText GetDisplayName() const override
	{
		return NSLOCTEXT("DisplayName", "AssetDefinition_HoudiniPreset", "Houdini Preset");
	}

	FText GetToolTip() const override
	{
		return NSLOCTEXT("Tooltip", "AssetDefinition_HoudiniPresetTooltip", "Contains a preset that can be applied to HDAs.");
	}

	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		UHoudiniPreset* Preset = nullptr;
		if (ensure(SupportedClass == Class))
		{
			ensure(0 != (RF_Public & Flags));
			Preset = NewObject<UHoudiniPreset>(InParent, Class, Name, Flags);
		}
		return Preset;
	}

	uint32 GetMenuCategories() const override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return AssetTools.RegisterAdvancedAssetCategory("HoudiniEngine", NSLOCTEXT("AssetCategoryName", "HoudiniEngineCategory", "Houdini Engine"));
	}

	FString GetDefaultNewAssetName() const override
	{
		return TEXT("HoudiniPreset");
	}

	bool ShouldShowInNewMenu() const override { return true; }
};



