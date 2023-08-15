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
#include "Engine/DeveloperSettings.h"
#include "UObject/Object.h"

#include "HoudiniToolTypes.h"

#include "HoudiniEngineEditorSettings.generated.h"

USTRUCT()
struct FUserPackageRules 
{
	GENERATED_BODY()

	FUserPackageRules()
		: ToolsPackageAsset(nullptr)
	{}

	// Tools package that will be used to resolve the include/exclude patterns.
	UPROPERTY(EditAnywhere, Category="User Category")
	class UHoudiniToolsPackageAsset* ToolsPackageAsset;

	// Include any tools that match the following patterns (relative to the Tools Package).
	UPROPERTY(EditAnywhere, Category="User Category")
	TArray<FString> Include;

	// Any tools matching these patterns (relative to the Tools Package) will be omitted from the inclusion set.
	UPROPERTY(EditAnywhere, Category="User Category")
	TArray<FString> Exclude;
};

UENUM()
enum class EHoudiniEngineEditorSettingUseCustomLocation : uint8
{
	// Use the per-project plugin settings.
	Project UMETA(DisplayName = "Use The Per-Project Plugin Settings"),
	// Do not use a custom location at all.
	Disabled UMETA(DisplayName = "Do Not Use Custom Location"),
	// Use the custom location defined here in editor preferences.
	Enabled UMETA(DisplayName = "Use Custom Location"),
};

USTRUCT()
struct FUserCategoryRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="User Category")
	TArray<FUserPackageRules> Packages;
};

UCLASS(config=HoudiniEngine)
class HOUDINIENGINERUNTIME_API UHoudiniEngineEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	DECLARE_MULTICAST_DELEGATE(FOnUserToolCategoriesChanged);
	
	UHoudiniEngineEditorSettings(const FObjectInitializer& ObjectInitializer);

	// Begin -- UDeveloperSettings interface
	
	// Gets the settings container name for the settings, either Project or Editor
	// We have to override the container name to Editor since we are using a custom config name in the UCLASS macro.
	virtual FName GetContainerName() const override { return TEXT("Editor"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	
	//End -- UDeveloperSettings interface

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	
#endif

#if WITH_EDITORONLY_DATA
	// User defined categories for HoudiniTools
	UPROPERTY(GlobalConfig, EditAnywhere, Category=HoudiniTools)
	TMap<FString, FUserCategoryRules> UserToolCategories;

	FOnUserToolCategoriesChanged OnUserToolCategoriesChanged;

	//-------------------------------------------------------------------------------------------------------------
	// Custom Houdini Location
	//-------------------------------------------------------------------------------------------------------------

	// Whether to use custom Houdini location from the project plugin settings, the editor setting, or do not use a custom location.
	UPROPERTY(config, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Use custom Houdini location (requires restart)", ConfigRestartRequired=true))
	EHoudiniEngineEditorSettingUseCustomLocation UseCustomHoudiniLocation;
	
	// Custom Houdini location (where HAPI library is located).
	UPROPERTY(config, EditAnywhere, Category = HoudiniLocation, Meta = (EditCondition = "UseCustomHoudiniLocation == EHoudiniEngineEditorSettingUseCustomLocation::Enabled", DisplayName = "Custom Houdini location", ConfigRestartRequired=true))
	FDirectoryPath CustomHoudiniLocation;
#endif
};
