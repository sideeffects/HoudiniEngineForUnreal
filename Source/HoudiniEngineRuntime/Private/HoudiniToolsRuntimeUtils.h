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
#include "HoudiniToolData.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolTypes.h"
#include "ImageCore.h"
#include "AssetRegistry/AssetRegistryModule.h"

// -----------------------------
// Houdini Tools 
// -----------------------------
// Houdini Tools maintains a cache of packages, tools, and categories. It also provided accessors for any UI
// tools to access this data

class FJsonObject;
struct FHoudiniToolList;
struct FSlateBrush;
class UHoudiniAsset;

struct FFilePath;
enum class EHoudiniToolSelectionType : uint8;
enum class EHoudiniToolType : uint8;
struct FHoudiniToolDirectory;
struct FHoudiniTool;
class UHoudiniToolsPackageAsset;

class HOUDINIENGINERUNTIME_API FHoudiniToolsRuntimeUtils
{
	protected:
	static IAssetRegistry& GetAssetRegistry();
	
public:
	FHoudiniToolsRuntimeUtils();

#if WITH_EDITOR
	// DECLARE_MULTICAST_DELEGATE(FOnToolChanged)
	
	
	
	// Compute the file path of where we expect this HoudiniAsset's JSON file resides. 
	static bool GetHoudiniAssetJSONPath(const UHoudiniAsset* HoudiniAsset, FString& OutJSONFilePath);
	
	static FString GetPackageUAssetName();
	static FString GetPackageJSONName();

	// Compute the default file path of where we expect this HoudiniAsset's JSON file resides. 
	static FString GetDefaultHoudiniAssetIconPath(const UHoudiniAsset* HoudiniAsset);
	
	static FString GetHoudiniToolsPackageJSONPath(const UHoudiniToolsPackageAsset* ToolsPackage);

	static bool LoadJSONFile(const FString& JSONFilePath, TSharedPtr<FJsonObject>& OutJSONObject);
	static bool WriteJSONFile(const FString& JSONFilePath, const TSharedPtr<FJsonObject> JSONObject);

	// Convert between enums and string values used in JSON files
	static bool ToolTypeToString(const EHoudiniToolType ToolType, FString& OutString);
	static bool SelectionTypeToString(const EHoudiniToolSelectionType SelectionType, FString& OutString);

	static UHoudiniToolsPackageAsset* LoadHoudiniToolsPackage(const FString& PackageBasePath);

	// From the current HoudiniAsset, search up the directory tree to see if we can find a
	// HoudiniToolsPackageAsset, since HoudiniAssets can live in subfolders in a package.
	// Returns the first (closest) HoudiniToolsPackageAsset found.
	static UHoudiniToolsPackageAsset* FindOwningToolsPackage(const UHoudiniAsset* HoudiniAsset);
	
	// Return the UHoudiniToolData object on the HoudiniAsset. If it doesn't exist, create it. 
	static UHoudiniToolData* GetOrCreateHoudiniToolData(UHoudiniAsset* HoudiniAsset);

	// External Data

	// Read HDA description
	static bool GetHoudiniPackageDescriptionFromJSON(
		const FString& JsonFilePath,
		TMap<FString, FCategoryRules>& OutCategories,
		bool& OutImportPackageDescription, bool& OutExportPackageDescription, bool& OutImportToolsDescription, bool&
		OutExportToolsDescription
	);

	// Make a default JSON object from the given Houdini Asset.
	static TSharedPtr<FJsonObject> MakeDefaultJSONObject(const UHoudiniAsset* HoudiniAsset);

	// Make a default JSON object from the given Houdini Tools Package.
	static TSharedPtr<FJsonObject> MakeDefaultJSONObject(const UHoudiniToolsPackageAsset* ToolsPackage);

	// Write a default JSON file for the given HoudiniAsset
	static bool WriteDefaultJSONFile(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath);

	// Write a default JSON file for the given HoudiniToolPackageAsset
	static bool WriteDefaultJSONFile(const UHoudiniToolsPackageAsset* ToolsPackage, const FString& JSONFilePath);

	// Use all the data that can be extracted from the UHoudiniAsset to write out a JSON
	// file, while preserving any existing JSON data.
	static bool WriteJSONFromHoudiniAsset(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath);

	// Use all the data that can be extracted from the UHoudiniAsset to write out a JSON
	// file, while preserving any existing JSON data.
	static bool WriteJSONFromToolPackageAsset(const UHoudiniToolsPackageAsset* ToolsPackage, const FString& JSONFilePath);

	// --------------------------------
	// Icons and Thumbnails
	// --------------------------------

	static void PadImage(const FImage& SrcImage, FImage& DestImage, const int32 NumPixels);

	static bool LoadFHImageFromFile(const FString& ImagePath, FHImageData& OutImageData);
	
	// Cache the HoudiniAsset's icon, if it has one, with the Thumbnail Manager.
	static void UpdateHoudiniAssetThumbnail(UHoudiniAsset* HoudiniAsset);
	static void UpdateAssetThumbnailFromImageData(UObject* Asset, const FHImageData& ImageData, const float MarginPct=0.2f);

	static bool ShowToolsPackageRenameConfirmDialog();

#endif
};
