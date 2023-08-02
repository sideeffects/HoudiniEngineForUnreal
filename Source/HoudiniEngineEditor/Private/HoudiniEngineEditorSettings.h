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
#include "UObject/Object.h"
#include "HoudiniToolTypes.h"

#include "HoudiniEngineEditorSettings.generated.h"

USTRUCT()
struct FUserCategoryRules 
{
	GENERATED_BODY()

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

UCLASS(config=EditorPerProjectUserSettings, defaultconfig)
class UHoudiniEngineEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	DECLARE_MULTICAST_DELEGATE(FOnUserToolCategoriesChanged);
	
	UHoudiniEngineEditorSettings();
	
	// // Additional user-specific search paths in the project to search for HoudiniTools packages.
	// // These paths should point to Tools directories that contain on or more HoudiniTools packages. It
	// // should not point to the HoudiniTools packages themselves.
	// UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniTools)
	// TArray<FString> HoudiniToolsSearchPath;
	
	// User defined categories for HoudiniTools
	UPROPERTY(GlobalConfig, EditAnywhere, Category=HoudiniTools)
	TMap<FString, FUserCategoryRules> UserToolCategories;

	FOnUserToolCategoriesChanged OnUserToolCategoriesChanged;

#if WITH_EDITOR
	
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	
#endif
};
