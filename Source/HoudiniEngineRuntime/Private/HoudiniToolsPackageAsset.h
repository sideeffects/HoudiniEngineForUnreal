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
#include "HoudiniToolTypes.h"
#include "UObject/Object.h"

#include "HoudiniToolsPackageAsset.generated.h"

enum class EAssetCommandResult : uint8;
struct FAssetCategoryPath;
struct FAssetOpenArgs;

/**
 * This is a Houdini Tools package descriptor inside of UE, typically created
 * after a HoudiniToolsPackage has been imported into the UE project.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class HOUDINIENGINERUNTIME_API UHoudiniToolsPackageAsset : public UObject
{
	GENERATED_BODY()
public:
	// Package data, typically extracted from imported JSON data.
	UHoudiniToolsPackageAsset();

	// Rename override - prevents renaming tools package
	virtual bool Rename(const TCHAR* NewName/* =nullptr */, UObject* NewOuter=nullptr, ERenameFlags Flags=REN_None) override;
	
	// Category names mapped to string patterns that will be used to determine whether
	// HDAs in this package is assigned to the respective category.
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	TMap<FString, FCategoryRules> Categories;

	// Optional path to an external directory containing HDAs associated with this package.
	// This path will be used to import new HDAs into this package.
	// This path is always relative to the Project File Path.
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	FDirectoryPath ExternalPackageDir;

	// If a package update/reimport is executed, reimport external package JSON data.
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	bool bReimportPackageDescription;

	// Any changes to this package description will be written to the package's external JSON file. If the external
	// JSON file doesn't exist, it will be created.
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	bool bExportPackageDescription;

	// When an HDA is reimported into this package, and it has an external JSON file, the data from the external
	// JSON file will be imported. NOTE: When HDA is imported into for the first time, it will always import
	// external JSON data, if it exists.
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	bool bReimportToolsDescription;

	// Any changes to HDAs in this package will be written to the HDA's external JSON file. If the external JSON file
	// doesn't exist, it will be created. 
	UPROPERTY(EditAnywhere, Category = "HoudiniToolsPackage")
	bool bExportToolsDescription;
	

#if WITH_EDITOR
	/**
	 * UObject overrides   
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;

private:
	// Helper function to convert SourcePackagePath to a path that is relative to the ProjectDir.  
	void MakeRelativePackagePath();

	// Apply property transformations if needed. 
	void UpdateProperties();
	
#endif
};



// // --------------------------------------------
// // HoudiniToolsPackageAsset factory
// // --------------------------------------------
//
// UCLASS(hidecategories = Object)
// class HOUDINIENGINERUNTIME_API UHoudiniToolsPackageAssetFactory : public UFactory
// {
// 	GENERATED_BODY()
//
// public:
//
// 	UHoudiniToolsPackageAssetFactory()
// 	{
// 		SupportedClass = UHoudiniToolsPackageAsset::StaticClass();
//
// 		bCreateNew = true;
// 		bEditAfterNew = true;
// 	}
//
// 	FText GetDisplayName() const override
// 	{
// 		return NSLOCTEXT("DisplayName", "AssetDefinition_HoudiniToolsPackage", "Houdini Tools Package");
// 	}
//
// 	FText GetToolTip() const override
// 	{
// 		return NSLOCTEXT("Tooltip", "AssetDefinition_HoudiniToolsPackage", "Describes a Houdini Tools package.");
// 	}
//
// 	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
// 	{
// 		UHoudiniToolsPackageAsset* Preset = nullptr;
// 		if (ensure(SupportedClass == Class))
// 		{
// 			ensure(0 != (RF_Public & Flags));
// 			Preset = NewObject<UHoudiniToolsPackageAsset>(InParent, Class, Name, Flags);
// 		}
// 		return Preset;
// 	}
//
// 	uint32 GetMenuCategories() const override
// 	{
// 		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
// 		return AssetTools.RegisterAdvancedAssetCategory("HoudiniEngine", NSLOCTEXT("AssetCategoryName", "AssetDefinition_HoudiniToolsPackage", "HoudiniEngine"));
// 	}
//
// 	FString GetDefaultNewAssetName() const override
// 	{
// 		return TEXT("Houdini Asset Package");
// 	}
//
// 	bool ShouldShowInNewMenu() const override { return true; }
// };


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
