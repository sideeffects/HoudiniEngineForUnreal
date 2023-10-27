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
#include "HoudiniAsset.h"
#include "HoudiniPreset.h"
#include "HoudiniToolTypes.h"

#include "HoudiniEngineToolTypes.generated.h"


class IAssetTools;
class IAssetTypeActions;
class IComponentAssetBroker;
class UHoudiniAssetComponent;
class FMenuBuilder;

struct FSlateBrush;
struct FHoudiniToolDescription;

USTRUCT(BlueprintType)
struct FHoudiniToolDirectory
{
	GENERATED_BODY()

	/** Name of the tool directory */
	UPROPERTY(GlobalConfig, Category = Tool, EditAnywhere)
	FString Name;

	/** Path of the tool directory */
	UPROPERTY(GlobalConfig, Category = Tool, EditAnywhere)
	FDirectoryPath Path;

	/** Unique generated ID used to store the imported uasset for the tools */
	UPROPERTY(GlobalConfig, Category = Tool, VisibleDefaultsOnly)
	FString ContentDirID;

	FORCEINLINE bool operator==(const FHoudiniToolDirectory& Other)const
	{
		return Name == Other.Name && Path.Path == Other.Path.Path;
	}

	FORCEINLINE bool operator!=(const FHoudiniToolDirectory& Other)const
	{
		return !(*this == Other);
	}
};


UENUM()
enum class EHoudiniToolCategoryType : uint8
{
	Package,
	User
};


UENUM()
enum class EHoudiniPackageToolType : uint8
{
	HoudiniAsset,
	Preset
};


USTRUCT(BlueprintType)
struct FHoudiniToolCategory
{
	GENERATED_BODY()

	FHoudiniToolCategory()
		: Name()
		, CategoryType(EHoudiniToolCategoryType::Package)
	{}

	FHoudiniToolCategory(const FString& InCategoryName, const EHoudiniToolCategoryType& InCategoryType)
		: Name(InCategoryName)
		, CategoryType(InCategoryType)
	{}
	
	bool operator<(const FHoudiniToolCategory& Other) const
	{
		if (CategoryType != Other.CategoryType)
		{
			// if we have different categories, the User category
			// should always be first in the list.
			if (CategoryType == EHoudiniToolCategoryType::User)
			{
				return true; // A < B
			}
			else
			{
				return false; // A > B
			}
		}

		// For now, we manually place the SideFX category at the top until
		// we have a more generic category prioritization mechanism. 
		if (Name.ToUpper() == TEXT("SIDEFX"))
			return true;
		if (Other.Name.ToUpper() == TEXT("SIDEFX"))
			return false;

		// Category Types are the same. Resolve priority by name.
		return Name < Other.Name;
	}

	bool operator==(const FHoudiniToolCategory& Other) const
	{
		return (Name == Other.Name) && (CategoryType == Other.CategoryType);
	}
	

	FHoudiniToolCategory& operator=(const FHoudiniToolCategory& Other) = default;
	
	FString Name;
	EHoudiniToolCategoryType CategoryType;
};

inline uint32 GetTypeHash(const FHoudiniToolCategory& Category)
{
	uint32 Hash = GetTypeHash(Category.Name);
	Hash = HashCombine(Hash, FCrc::TypeCrc32(Category.CategoryType));
	return Hash;
}


struct FHoudiniTool
{
	
	FHoudiniTool()
		: HoudiniAsset( nullptr)
		, Name()
		, ToolTipText()
		, Icon()
		, IconTexture(nullptr)
		, HelpURL()
		, Type(EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
		, CategoryType(EHoudiniToolCategoryType::Package)
		, SelectionType(EHoudiniToolSelectionType::HTOOL_SELECTION_ALL)
	{
	}

	FHoudiniTool(
		const TSoftObjectPtr < class UHoudiniAsset >& InHoudiniAsset, const FText& InName,
		const EHoudiniToolType& InType, const EHoudiniToolSelectionType& InSelType,
		const FText& InToolTipText, TSharedPtr<FSlateBrush> InIcon,
		UTexture2D* InIconTexture,
		const FString& InHelpURL,
		const EHoudiniToolCategoryType& InCategoryType, const FHoudiniToolDirectory& InToolDirectory,
		const FString& InJSONFile )
		: HoudiniAsset( InHoudiniAsset )
		, Name( InName )
		, ToolTipText( InToolTipText )
		, Icon( InIcon )
		, IconTexture( InIconTexture )
		, HelpURL( InHelpURL )
		, Type( InType )
		, CategoryType( InCategoryType )
		, SelectionType( InSelType )
	{
	}

	// Return the asset object based on the PackageToolType. This will load the object if needed.
	UObject* GetAssetObject() const
	{
		switch (PackageToolType)
		{
			case EHoudiniPackageToolType::HoudiniAsset:
				return HoudiniAsset.LoadSynchronous();
				break;
			case EHoudiniPackageToolType::Preset:
				return HoudiniPreset.LoadSynchronous();
				break;
			default: ;
		}
		return nullptr;
	}

	UClass* GetAssetObjectClass() const
	{
		switch (PackageToolType)
		{
			case EHoudiniPackageToolType::HoudiniAsset:
				return UHoudiniAsset::StaticClass();
				break;
			case EHoudiniPackageToolType::Preset:
				return UHoudiniPreset::StaticClass();
				break;
			default: ;
		}
		return nullptr;
	}

	bool IsHiddenInCategory(const FString& CategoryName) const { return ExcludedFromCategories.Contains(CategoryName); }

	/** The Houdini Asset used by the tool **/ 
	TSoftObjectPtr < UHoudiniAsset > HoudiniAsset;
	/** The (optional) Houdini Preset represent by this tool, if it is a present **/
	TSoftObjectPtr < UHoudiniPreset > HoudiniPreset;
	/** The owning tools package **/
	TSoftObjectPtr < class UHoudiniToolsPackageAsset > ToolsPackage;

	// Type for this tool inside a package
	EHoudiniPackageToolType PackageToolType;

	/** The name to be displayed */
	FText Name;

	/** The name to be displayed */
	FText ToolTipText;

	/** The icon to be displayed */
	TSharedPtr<FSlateBrush> Icon;

	UTexture2D* IconTexture;

	/** The help URL for this tool */
	FString HelpURL;

	/** The type of tool, this will change how the asset handles the current selection **/
	EHoudiniToolType Type;

	// Type of category to which FHoudiniTool belongs.
	EHoudiniToolCategoryType CategoryType;

	/** Indicate what the tool should consider for selection **/
	EHoudiniToolSelectionType SelectionType;

	// Categories from which this HoudiniTool is explicitly excluded / hidden
	TSet<FString> ExcludedFromCategories;
};

struct FHoudiniToolList
{
	// List of HoudiniTools
	TArray<TSharedPtr<FHoudiniTool>> Tools;
	// Set of unique packages collected from the tools in this list. 
	TSet<TSoftObjectPtr<UHoudiniToolsPackageAsset>> Packages;
};





