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

#include "HoudiniToolsRuntimeUtils.h"

#if WITH_EDITOR
#include "HoudiniAsset.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniToolsPackageAsset.h"
#include "ImageUtils.h"
#include "HoudiniToolData.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/MessageDialog.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#include "ObjectTools.h"

#ifndef ENGINE_MAJOR_VERSION
#include "Runtime/Launch/Resources/Version.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define DEFAULT_GAMMA_SPACE EGammaSpace::Invalid
#define DEFAULT_RAW_IMAGE_FORMAT_TYPE ERawImageFormat::Invalid
#else
#define DEFAULT_GAMMA_SPACE EGammaSpace::sRGB
#define DEFAULT_RAW_IMAGE_FORMAT_TYPE ERawImageFormat::Type::BGRA8
#endif

#define LOCTEXT_NAMESPACE "HoudiniTools"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
// Unreal 5.0 implementation
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

struct FToolsWrapper
{
	
	static bool LoadImage(const TCHAR * Filename, FImage & OutImage)
	{
		// implementation adapted from: FSlateRHIResourceManager::LoadTexture
		bool bSucceeded = false;
		uint32 BytesPerPixel = 4;

		TArray<uint8> RawFileData;
		if( FFileHelper::LoadFileToArray( RawFileData, Filename ) )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

			//Try and determine format, if that fails assume PNG
			EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
			if (ImageFormat == EImageFormat::Invalid)
			{
				ImageFormat = EImageFormat::PNG;
			}

			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num()) )
			{
				OutImage.SizeX = ImageWrapper->GetWidth();
				OutImage.SizeY = ImageWrapper->GetHeight();
				OutImage.Format = DEFAULT_RAW_IMAGE_FORMAT_TYPE;
				OutImage.NumSlices = 1;
				
				if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, OutImage.RawData))
				{
					bSucceeded = true;
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), Filename );
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Only pngs are supported in Slate."));
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find file: %s"), Filename);
		}

		return bSucceeded;
	}
	

};

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
// Unreal 5.1+ implementation

struct FToolsWrapper
{
    static bool LoadImage(const TCHAR * Filename, FImage & OutImage)
    {
	    return FImageUtils::LoadImage(Filename, OutImage);
    }
};

#endif


IAssetRegistry&
FHoudiniToolsRuntimeUtils::GetAssetRegistry()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
}


FHoudiniToolsRuntimeUtils::FHoudiniToolsRuntimeUtils()
{
	
}


bool
FHoudiniToolsRuntimeUtils::GetHoudiniAssetJSONPath(const UHoudiniAsset* HoudiniAsset, FString& OutJSONFilePath)
{
	if (!IsValid(HoudiniAsset))
	{
		return false;
	}

	if (!IsValid(HoudiniAsset->AssetImportData))
	{
		return false;
	}
	
	// Compute the filepath of where we expect this HoudiniAsset's json file resides.
	const FString& ReimportPath = HoudiniAsset->AssetImportData->GetFirstFilename();
	FString Path, Filename, Extension;
	FPaths::Split(ReimportPath, Path, Filename, Extension);
	OutJSONFilePath = FPaths::Combine(Path, Filename + ".json");

	return true;
}


FString
FHoudiniToolsRuntimeUtils::GetPackageUAssetName()
{
	return TEXT("HoudiniToolsPackage");
}


FString
FHoudiniToolsRuntimeUtils::GetPackageJSONName()
{
	return TEXT("HoudiniToolsPackage.json");
}


FString
FHoudiniToolsRuntimeUtils::GetDefaultHoudiniAssetIconPath(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
		return FString();

	if (!IsValid(HoudiniAsset->AssetImportData))
		return FString();
	
	// Compute the filepath of where we expect this HoudiniAsset's json file resides.
	const FString& ReimportPath = HoudiniAsset->AssetImportData->GetFirstFilename();
	FString Path, Filename, Extension;
	FPaths::Split(ReimportPath, Path, Filename, Extension);
	return FPaths::Combine(Path, Filename + ".png");
}


FString
FHoudiniToolsRuntimeUtils::GetHoudiniToolsPackageJSONPath(
	const UHoudiniToolsPackageAsset* ToolsPackage)
{
	if (!IsValid(ToolsPackage))
	{
		return FString();
	}
	
	return FPaths::Combine(FPaths::GetPath(FPaths::GetProjectFilePath()), ToolsPackage->ExternalPackageDir.Path, GetPackageJSONName());
}


bool
FHoudiniToolsRuntimeUtils::LoadJSONFile(const FString& JSONFilePath, TSharedPtr<FJsonObject>& OutJSONObject)
{
	if ( JSONFilePath.IsEmpty() )
		return false;

	// Read the file as a string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString( FileContents, *JSONFilePath ) )
	{
		HOUDINI_LOG_ERROR( TEXT("Failed to import Package Descriptor .json file: %s"), *JSONFilePath);
		return false;
	}

	// Parse it as JSON
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );
	if (!FJsonSerializer::Deserialize(Reader, OutJSONObject) || !OutJSONObject.IsValid())
	{
		HOUDINI_LOG_ERROR( TEXT("Invalid json file: %s"), *JSONFilePath);
		return false;
	}

	return true;
}


bool
FHoudiniToolsRuntimeUtils::WriteJSONFile(const FString& JSONFilePath, const TSharedPtr<FJsonObject> JSONObject)
{
	// Output the JSON to a String
	FString OutputString;
	const TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JSONObject.ToSharedRef(), Writer);

	// Then write the output string to the json file itself
	const FString SaveDirectory = FPaths::GetPath(JSONFilePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	// Returns true if the directory existed or has been created
	if ( PlatformFile.CreateDirectoryTree(*SaveDirectory) )
	{
		return FFileHelper::SaveStringToFile(OutputString, *JSONFilePath);
	}
	
	return false;
}


bool
FHoudiniToolsRuntimeUtils::ToolTypeToString(const EHoudiniToolType ToolType, FString& OutString)
{
	switch(ToolType)
	{
		case EHoudiniToolType::HTOOLTYPE_GENERATOR:
		{
			OutString = TEXT("GENERATOR");
			return true;
		}
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE:
		{
			OutString = TEXT("OPERATOR_SINGLE");
			return true;
		}
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI:
		{
			OutString = TEXT("OPERATOR_MULTI");
			return true;
		}
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH:
		{
			OutString = TEXT("BATCH");
			return true;
		}
	default:
		break;
	}
	return false;
}


bool
FHoudiniToolsRuntimeUtils::SelectionTypeToString(const EHoudiniToolSelectionType SelectionType, FString& OutString)
{
	switch (SelectionType)
	{
		case EHoudiniToolSelectionType::HTOOL_SELECTION_ALL:
		{
			OutString = TEXT("ALL");
			return true;
		}
		case EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY:
		{
			OutString = TEXT("CB");
			return true;
		}
		case EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY:
		{
			OutString = TEXT("CB");
			return true;
		}
		default:
			break;
	}

	return false;
}


UHoudiniToolsPackageAsset*
FHoudiniToolsRuntimeUtils::LoadHoudiniToolsPackage(const FString& PackageBasePath)
{
	// No need to load/find Tools package while cooking or running a commandlet
	// This would only generate unneeded warnings
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || GIsCookerLoadingPackage)
		return nullptr;

	const FString PkgPath = FPaths::Combine(PackageBasePath, FString::Format(TEXT("{0}.{0}"), { GetPackageUAssetName() }) );
	return LoadObject<UHoudiniToolsPackageAsset>(nullptr, *PkgPath);
}


UHoudiniToolsPackageAsset*
FHoudiniToolsRuntimeUtils::FindOwningToolsPackage(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
		return nullptr;

	// No need to load/find Tools package while cooking or running a commandlet
	// This would only generate unneeded warnings
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || GIsCookerLoadingPackage)
		return nullptr;

	FString CurrentPath = FPaths::GetPath(HoudiniAsset->GetPathName());

	// Define a depth limit to break out of the loop, in case something
	// goes wrong and we get stuck. If ever needed, we can move this to a
	// project setting, but for now we'll leave it here.
	constexpr int LoopLimit = 100;
	int LoopCount = 0;
	
	while (CurrentPath.Len() > 0)
	{
		// Try to load a package at the current path.
		UHoudiniToolsPackageAsset* PackageAsset = LoadHoudiniToolsPackage(CurrentPath);
		if (IsValid(PackageAsset))
		{
			// We found a valid package asset.
			return PackageAsset;
		}

		// Proceed to the next parent directory.
		CurrentPath = FPaths::GetPath(CurrentPath);
		
		LoopCount++;
		if (LoopCount >= LoopLimit)
		{
			return nullptr;
		}
	}

	return nullptr;
}


UHoudiniToolData*
FHoudiniToolsRuntimeUtils::GetOrCreateHoudiniToolData(UHoudiniAsset* HoudiniAsset)
{
	UHoudiniToolData* ToolData = HoudiniAsset->HoudiniToolData;

	if (!IsValid(ToolData))
	{
		HoudiniAsset->Modify();
		ToolData = NewObject<UHoudiniToolData>(HoudiniAsset);
		ToolData->Modify();
		HoudiniAsset->HoudiniToolData = ToolData;
	}

	return ToolData;
}


bool
FHoudiniToolsRuntimeUtils::GetHoudiniPackageDescriptionFromJSON(const FString& JsonFilePath,
	TMap<FString, FCategoryRules>& OutCategories,
	bool& OutImportPackageDescription,
	bool& OutExportPackageDescription,
	bool& OutImportToolsDescription,
	bool& OutExportToolsDescription
	)
{
	if ( JsonFilePath.IsEmpty() )
		return false;

	// Read the file as a string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString( FileContents, *JsonFilePath ) )
	{
		HOUDINI_LOG_ERROR( TEXT("Failed to import Package Descriptor .json file: %s"), *JsonFilePath);
		return false;
	}

	// Parse it as JSON
	TSharedPtr<FJsonObject> JSONObject;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );
	if (!FJsonSerializer::Deserialize(Reader, JSONObject) || !JSONObject.IsValid())
	{
		HOUDINI_LOG_ERROR( TEXT("Invalid json in Houdini Tool .json file: %s"), *JsonFilePath);
		return false;
	}

	// Categories
	OutCategories.Empty();

	// Then, we need to make sure the json file has a correponding hda
	// Start by looking at the assetPath field
	FString HDAFilePath = FString();
	if ( JSONObject->HasField( TEXT("categories") ) )
	{
		// If the json has the assetPath field, read it from there
		TArray<TSharedPtr<FJsonObject>> CategoryEntries;
		
		const TArray<TSharedPtr<FJsonValue>>* CategoriesArray = nullptr;
		if (JSONObject->TryGetArrayField(TEXT("categories"), CategoriesArray))
		{
			for (auto Value : *CategoriesArray)
			{
				CategoryEntries.Add(Value->AsObject());
			}
		}
		
		for(TSharedPtr<FJsonObject>& CategoryEntry : CategoryEntries)
		{
			FString CategoryName;
			// Category rules that will be added to the output.
			FCategoryRules OutCategoryRules;

			if (CategoryEntry->HasField(TEXT("name")))
			{
				CategoryName = CategoryEntry->GetStringField(TEXT("name"));
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Found category entry without 'name' field. Skipping."));
				continue;
			}
			
			if (!CategoryEntry->TryGetStringArrayField(TEXT("include"), OutCategoryRules.Include))
			{
				// Categories should really have an include section, so we'll print a warning here.
				HOUDINI_LOG_WARNING(TEXT("Could not parse 'include' string array for category '%s'."), *CategoryName);
			}

			// Exclude sections are optional, so just quietly ignore it if its not present.
			CategoryEntry->TryGetStringArrayField(TEXT("exclude"), OutCategoryRules.Exclude);

			OutCategories.Add(CategoryName, OutCategoryRules);

			HOUDINI_LOG_WARNING(TEXT("[GetHoudiniPackageDescriptionFromJSON] Category '%s': includes %d, excludes, %d"), *CategoryName, OutCategoryRules.Include.Num(), OutCategoryRules.Exclude.Num());
		}
	}

	auto GetBoolFieldFn = [JSONObject](const FString& FieldName, const bool DefaultValue) -> bool
	{
		if ( JSONObject->HasField( FieldName ) )
		{
			return JSONObject->GetBoolField(FieldName);
		}
		return DefaultValue;
	};

	// By default, we won't enable importing and exporting of JSON data data for both packages and tools. This
	// prevents situations where "default use" of the plugin starts creating external files. Instead, for pipelines
	// that prefer this behaviour, it can be easily enabled in the package descriptor.
	OutImportPackageDescription = GetBoolFieldFn(TEXT("reimport_package_description"), false);
	OutExportPackageDescription = GetBoolFieldFn(TEXT("export_package_description"), false);
	OutImportToolsDescription = GetBoolFieldFn(TEXT("reimport_tools_description"), false);
	OutExportToolsDescription = GetBoolFieldFn(TEXT("export_tools_description"), false);
	return true;
}


TSharedPtr<FJsonObject>
FHoudiniToolsRuntimeUtils::MakeDefaultJSONObject(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
		return nullptr;
	
	// Start by building a JSON object from the tool
	TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject);

	// Mark the target as unreal only
	TArray< TSharedPtr<FJsonValue> > TargetValue;
	TargetValue.Add(MakeShareable(new FJsonValueString(TEXT("unreal"))));
	JSONObject->SetArrayField(TEXT("target"), TargetValue );

	const UHoudiniToolData* ToolData = HoudiniAsset->HoudiniToolData;
	const bool bValidToolData = IsValid(ToolData); 

	// The Tool Name
	const FString ToolName = bValidToolData ? ToolData->Name : FPaths::GetBaseFilename(HoudiniAsset->AssetFileName); 
	JSONObject->SetStringField(TEXT("name"), ToolName);

	// Tooltype
	FString ToolTypeString;
	const EHoudiniToolType ToolType = bValidToolData ? ToolData->Type : EHoudiniToolType::HTOOLTYPE_GENERATOR;
	ToolTypeToString(ToolType, ToolTypeString);    
	JSONObject->SetStringField(TEXT("toolType"), ToolTypeString);

	// Tooltip
	if ( bValidToolData && !ToolData->ToolTip.IsEmpty() )
		JSONObject->SetStringField(TEXT("toolTip"), ToolData->ToolTip);

	// Help URL
	if ( bValidToolData && !ToolData->HelpURL.IsEmpty() )
		JSONObject->SetStringField(TEXT("helpURL"), ToolData->HelpURL);

	// // IconPath
	// if ( bValidToolData && !ToolData->IconResource )
	// {
	//     const FString IconPath = ToolData->IconResource->GetName();
	//     JSONObject->SetStringField(TEXT("iconPath"), IconPath);
	// }

	// Selection Type
	FString SelectionTypeString;
	const EHoudiniToolSelectionType SelectionType = bValidToolData ? ToolData->SelectionType : EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
	FHoudiniToolsRuntimeUtils::SelectionTypeToString(SelectionType, SelectionTypeString);
	JSONObject->SetStringField(TEXT("UE_SelectionType"), SelectionTypeString);

	return JSONObject;
}


TSharedPtr<FJsonObject>
FHoudiniToolsRuntimeUtils::MakeDefaultJSONObject(const UHoudiniToolsPackageAsset* ToolsPackage)
{
	if (!IsValid(ToolsPackage))
		return nullptr;

	// A default JSON file for a tools package will contain a single category using the directory
	// name of the ToolsPackage.
	// All reimport/export settings will be disabled by default.

	FString AssetPath = FPaths::GetPath(ToolsPackage->GetPathName());

	const FString PackageName = TEXT("PackageName");
	
	// Start by building a JSON object from the tool
	TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject);
	
	// Add an array for category entries
	TArray< TSharedPtr<FJsonValue> > CategoryEntries;

	// Populate a default category with an include entry, and an empty exclude list.
	const TSharedPtr<FJsonObject> CategoryEntry = MakeShareable(new FJsonObject);
	CategoryEntry->SetStringField(TEXT("name"), PackageName);

	TArray< TSharedPtr<FJsonValue> > IncludeValues;
	IncludeValues.Add(MakeShareable( new FJsonValueString(TEXT("*")) ));
	CategoryEntry->SetArrayField(TEXT("include"), IncludeValues);

	const TArray< TSharedPtr<FJsonValue> > ExcludeValues;
	CategoryEntry->SetArrayField(TEXT("exclude"), ExcludeValues);

	CategoryEntries.Add( MakeShareable( new FJsonValueObject(CategoryEntry) ) );

	// Add the default category entry.
	JSONObject->SetArrayField(TEXT("categories"), CategoryEntries);

	// Set defaults for the reimport/export settings.

	JSONObject->SetBoolField(TEXT("export_package_description"), false);
	JSONObject->SetBoolField(TEXT("reimport_package_description"), false);

	JSONObject->SetBoolField(TEXT("reimport_tools_description"), false);
	JSONObject->SetBoolField(TEXT("export_tools_description"), false);

	return JSONObject;
}


bool
FHoudiniToolsRuntimeUtils::WriteDefaultJSONFile(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath)
{
	if (!IsValid(HoudiniAsset))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> JSONObject = MakeDefaultJSONObject(HoudiniAsset);
	if (!JSONObject.IsValid())
	{
		return false;
	}

	return WriteJSONFile(JSONFilePath, JSONObject);
}


bool
FHoudiniToolsRuntimeUtils::WriteDefaultJSONFile(const UHoudiniToolsPackageAsset* ToolsPackage, const FString& JSONFilePath)
{
	if (!IsValid(ToolsPackage))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> JSONObject = MakeDefaultJSONObject(ToolsPackage);
	if (!JSONObject.IsValid())
	{
		return false;
	}

	return WriteJSONFile(JSONFilePath, JSONObject);
}


bool
FHoudiniToolsRuntimeUtils::WriteJSONFromHoudiniAsset(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath)
{
	if (!IsValid(HoudiniAsset))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JSONObject;
	if (FPaths::FileExists(JSONFilePath))
	{
		// This asset already has a JSON file.
		if (!LoadJSONFile(JSONFilePath, JSONObject))
		{
			// We were unable to parse it, so we'll fail here. We don't want to overwrite
			// files if we can't preserve its contents.
			HOUDINI_LOG_ERROR(TEXT("Unable to parse JSON file. We don't want to overwrite and possibly lose data. Skipping JSON data export for this asset."));
			return false;
		}
	}
	
	if (!JSONObject.IsValid())
	{
		// We don't have a valid JSON object, it means we should be creating a new default JSON object for this asset
		JSONObject = MakeDefaultJSONObject(HoudiniAsset);
		if (!JSONObject.IsValid())
		{
			return false;
		}
	}

	// NOTE: If we start writing out HDA JSON files in multiple places we should move the following
	// into a wrapper class with an specific accessors so that the field manipulations can be unified in a single place.

	FString Name;
	FString Tooltip;
	EHoudiniToolType ToolType;
	EHoudiniToolSelectionType SelectionType;
	FString HelpURL;
	FString IconSourcePath;
	
	// Populate the JSON object from the Houdini Asset.
	if (IsValid(HoudiniAsset->HoudiniToolData))
	{
		// Populate from ToolData (this is our preferred data source) 
		UHoudiniToolData* ToolData = HoudiniAsset->HoudiniToolData;
		
		Name = ToolData->Name;
		Tooltip = ToolData->ToolTip;
		// We don't really know what this HDA is, so we're just going with "no inputs" as a default.
		ToolType = ToolData->Type;
		SelectionType = ToolData->SelectionType;
		HelpURL = ToolData->HelpURL;
		IconSourcePath = ToolData->IconSourcePath.FilePath;
	}
	else
	{
		// Populate from any data we can extract from UHoudiniAsset without looking outside the Unreal project.
		// i.e., no peeking at external json files.        
		Name = FPaths::GetBaseFilename(HoudiniAsset->AssetFileName);
		Tooltip = Name;
		// We don't really know what this HDA is, so we're just going with "no inputs" as a default.
		ToolType = EHoudiniToolType::HTOOLTYPE_GENERATOR;
		SelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
	}

	// Mark the target as unreal only
	TArray< TSharedPtr<FJsonValue> > TargetValue;
	TargetValue.Add(MakeShareable(new FJsonValueString(TEXT("unreal"))));
	JSONObject->SetArrayField(TEXT("target"), TargetValue );

	// Tool Name
	if ( !Name.IsEmpty() )
	{
		JSONObject->SetStringField(TEXT("name"), Name);
	}

	// Tool Type
	FString ToolTypeString = TEXT("GENERATOR");
	if ( ToolType == EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
		ToolTypeString = TEXT("OPERATOR_SINGLE");
	else if (ToolType == EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI)
		ToolTypeString = TEXT("OPERATOR_MULTI");
	else if ( ToolType == EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH)
		ToolTypeString = TEXT("BATCH");

	JSONObject->SetStringField(TEXT("toolType"), ToolTypeString);

	// Tooltip
	JSONObject->SetStringField(TEXT("toolTip"), Tooltip);

	// Help URL
	JSONObject->SetStringField(TEXT("helpURL"), HelpURL);

	// IconPath
	bool bRemoveIconPath = true;
	if ( !IconSourcePath.IsEmpty() )
	{
		// Only write out the icon path if it differs from the 'default' icon path for the asset.
		const FString DefaultIconPath = GetDefaultHoudiniAssetIconPath(HoudiniAsset);
		if (IconSourcePath.Compare(DefaultIconPath) != 0)
		{
			JSONObject->SetStringField(TEXT("iconPath"), IconSourcePath);
			bRemoveIconPath = false;
		}
	}
	if (bRemoveIconPath)
	{
		JSONObject->RemoveField(TEXT("iconPath"));
	}

	// Selection Type
	FString SelectionTypeString = TEXT("All");
	if (SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY)
		SelectionTypeString = TEXT("CB");
	else if (SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY)
		SelectionTypeString = TEXT("World");
	JSONObject->SetStringField(TEXT("UE_SelectionType"), SelectionTypeString);

	WriteJSONFile(JSONFilePath, JSONObject);

	return true;
}


bool FHoudiniToolsRuntimeUtils::WriteJSONFromToolPackageAsset(const UHoudiniToolsPackageAsset* ToolsPackage,
	const FString& JSONFilePath)
{
	TSharedPtr<FJsonObject> JSONObject;
	if (FPaths::FileExists(JSONFilePath))
	{
		// This asset already has a JSON file.
		if (!LoadJSONFile(JSONFilePath, JSONObject))
		{
			// We were unable to parse it, so we'll fail here. We don't want to override
			// files if we can't preserve its contents.
			return false;
		}
	}
	
	if (!JSONObject.IsValid())
	{
		// We don't have a valid JSON object, it means we should be creating a new default JSON object for this asset
		JSONObject = MakeDefaultJSONObject(ToolsPackage);
		if (!JSONObject.IsValid())
		{
			return false;
		}
	}

	// Populate the JSON Object from the asset
	
	if (ToolsPackage->Categories.Num() > 0)
	{
		// Add an array for category entries
		TArray< TSharedPtr<FJsonValue> > CategoryValues;

		for( auto Entry : ToolsPackage->Categories)
		{
			// Create a Category entry
			const FString& CategoryName = Entry.Key;
			const FCategoryRules& CategoryRules = Entry.Value;
			
			TSharedPtr<FJsonObject> CategoryObject = MakeShareable(new FJsonObject);
			CategoryObject->SetStringField(TEXT("name"), CategoryName);

			// include patterns
			if (CategoryRules.Include.Num() > 0)
			{
				TArray< TSharedPtr<FJsonValue> > IncludeValues;
				for (const FString& Pattern : CategoryRules.Include)
				{
					IncludeValues.Add(MakeShareable( new FJsonValueString(Pattern) ));                    
				}
				CategoryObject->SetArrayField(TEXT("include"), IncludeValues);
			}

			// exclude patterns
			if (CategoryRules.Include.Num() > 0)
			{
				TArray< TSharedPtr<FJsonValue> > ExcludeValues;
				for (const FString& Pattern : CategoryRules.Exclude)
				{
					ExcludeValues.Add(MakeShareable( new FJsonValueString(Pattern) ));
				}
				CategoryObject->SetArrayField(TEXT("exclude"), ExcludeValues);
			}

			CategoryValues.Add(MakeShareable(new FJsonValueObject(CategoryObject)));
		}
		
		JSONObject->SetArrayField(TEXT("categories"), CategoryValues);
	}

	JSONObject->SetBoolField(TEXT("export_package_description"), ToolsPackage->bExportPackageDescription);
	JSONObject->SetBoolField(TEXT("reimport_package_description"), ToolsPackage->bReimportPackageDescription);

	JSONObject->SetBoolField(TEXT("export_tools_description"), ToolsPackage->bReimportToolsDescription);
	JSONObject->SetBoolField(TEXT("reimport_tools_description"), ToolsPackage->bExportToolsDescription);
	
	WriteJSONFile(JSONFilePath, JSONObject);

	return true;
}

void FHoudiniToolsRuntimeUtils::PadImage(const FImage& SrcImage, FImage& DestImage, const int32 NumPixels)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
	const int32 BytesPerPixel = SrcImage.GetBytesPerPixel();
#else
	const int32 BytesPerPixel = ERawImageFormat::GetBytesPerPixel(SrcImage.Format);
#endif

	DestImage.SizeX = SrcImage.SizeX + NumPixels*2;
	DestImage.SizeY = SrcImage.SizeY + NumPixels*2;
	DestImage.NumSlices = 1;
	DestImage.Format = SrcImage.Format;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
	DestImage.GammaSpace = SrcImage.GammaSpace;
#else
	DestImage.GammaSpace = SrcImage.GetGammaSpace();
#endif

	// Init dest image storage
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
	const int64 NumBytes = DestImage.SizeX * DestImage.SizeY * DestImage.NumSlices * BytesPerPixel;
#else
	const int64 NumBytes = DestImage.GetImageSizeBytes();
#endif
	DestImage.RawData.Empty(NumBytes);
	DestImage.RawData.AddZeroed(NumBytes);
	
	// Copy the source image into the center of the destination image
	const int32 SourceRowBytes = SrcImage.SizeX * BytesPerPixel;
	for (int Row = 0; Row < SrcImage.SizeY; Row++)
	{
		const int SourceIndex = (Row * SrcImage.SizeX) * BytesPerPixel;
		const int DestIndex = (NumPixels + ((Row+NumPixels) * DestImage.SizeX)) * BytesPerPixel;
		memcpy(&DestImage.RawData[DestIndex], &SrcImage.RawData[SourceIndex], SourceRowBytes );
	}
}


bool
FHoudiniToolsRuntimeUtils::LoadFHImageFromFile(const FString& ImagePath, FHImageData& OutImageData)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(ImagePath);
	bool bSuccess = false;
	
    if (FPaths::FileExists(FullPath))
    {
        FImage TmpImage;
		if (FToolsWrapper::LoadImage(*FullPath, TmpImage))
		{
	        OutImageData.FromImage(TmpImage);
			return true;
		}
    }
	return false;
}


void
FHoudiniToolsRuntimeUtils::UpdateHoudiniAssetThumbnail(UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset) || !IsValid(HoudiniAsset->HoudiniToolData))
	{
		return;
	}

	const FHImageData& IconImageData = HoudiniAsset->HoudiniToolData->IconImageData; 

	UpdateAssetThumbnailFromImageData(HoudiniAsset, HoudiniAsset->HoudiniToolData->IconImageData);
}


void
FHoudiniToolsRuntimeUtils::UpdateAssetThumbnailFromImageData(UObject* Asset, const FHImageData& ImageData, const float MarginPct)
{
	if (!IsValid(Asset))
	{
		return;
	}

	const FString ObjectFullName = Asset->GetFullName();
	UPackage* AssetPackage = Asset->GetPackage();
	if (!IsValid(AssetPackage))
	{
		return;
	}

	if (ImageData.SizeX == 0 || ImageData.SizeY == 0)
	{
		// Clear the thumbnail
		ThumbnailTools::CacheEmptyThumbnail(ObjectFullName, AssetPackage);
		return;
	}

	if (ImageData.SizeX == 0 || ImageData.SizeY == 0)
	{
		// Clear the thumbnail
		ThumbnailTools::CacheEmptyThumbnail(ObjectFullName, AssetPackage);
		return;
	}

	FImage SrcImage;
	ImageData.ToImage(SrcImage);

	// We can expose the margin as a parameter if we want/need to.
	if (MarginPct > 0.f)
	{
		// We can't configure a margin on the FImage, so we'll pad the image with
		// zero-byte pixels to give the illusion that we're applying a margin.
		FImage PaddedImage;
		PadImage(SrcImage, PaddedImage, SrcImage.SizeX*MarginPct);
		SrcImage = PaddedImage;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
	const int32 BytesPerPixel = SrcImage.GetBytesPerPixel();
#else
	const int32 BytesPerPixel = ERawImageFormat::GetBytesPerPixel(SrcImage.Format);
#endif

	FObjectThumbnail TempThumbnail;
	TempThumbnail.SetImageSize( SrcImage.SizeX, SrcImage.SizeY );
	TArray<uint8>& ThumbnailData = TempThumbnail.AccessImageData();

	const int32 NumBytes = SrcImage.SizeX * SrcImage.SizeY * BytesPerPixel * SrcImage.NumSlices;

	ThumbnailData.AddUninitialized(NumBytes);
	FMemory::Memcpy(&(ThumbnailData[0]), &(SrcImage.RawData[0]), NumBytes);
	
	FObjectThumbnail* NewThumbnail = ThumbnailTools::CacheThumbnail(ObjectFullName, &TempThumbnail, AssetPackage);
	if ( ensure(NewThumbnail) )
	{
		//we need to indicate that the package needs to be resaved
		AssetPackage->MarkPackageDirty();

		// Let the content browser know that we've changed the thumbnail
		NewThumbnail->MarkAsDirty();
			
		// Signal that the asset was changed so thumbnail pools will update
		Asset->PostEditChange();

		//Set that thumbnail as a valid custom thumbnail so it'll be saved out
		NewThumbnail->SetCreatedAfterCustomThumbsEnabled();
	}
}

bool
FHoudiniToolsRuntimeUtils::ShowToolsPackageRenameConfirmDialog()
{
	FText WarningMessage(LOCTEXT("Warning_ToolsPackageRename", "Naming a HoudiniToolsPackage to anything but \"HoudiniToolsPackage\" disables it - Are you sure you want to proceed with the rename?"));
	EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage);// , & MessageTitle);
	if (ReturnType == EAppReturnType::Yes)
	{
		return true;
	}
	else
	{
		return false;
	}
}

#endif

#undef DEFAULT_GAMMA_SPACE
#undef DEFAULT_RAW_IMAGE_FORMAT_TYPE
#undef LOCTEXT_NAMESPACE
