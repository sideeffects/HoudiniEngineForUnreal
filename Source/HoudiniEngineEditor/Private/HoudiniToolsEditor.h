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
#include "HoudiniEngineToolTypes.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniPreset.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolTypes.h"
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
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniTools, Log, All);

struct FFilePath;
enum class EHoudiniToolSelectionType : uint8;
enum class EHoudiniToolType : uint8;
struct FHoudiniToolDirectory;
struct FHoudiniTool;
class UHoudiniToolsPackageAsset;


class FHoudiniToolsEditor
{
	protected:
	static IAssetRegistry& GetAssetRegistry();
	
public:
	FHoudiniToolsEditor();

	// Returns the Default Icon to be used by Houdini Tools
	static FString GetDefaultHoudiniToolIconPath();
	static FString GetDefaultHoudiniToolIconBrushName();

	static FString GetAbsoluteGameContentDir();

	static FString GetDefaultHoudiniToolsPath();
	static FString GetDefaultPackagePath(const FString& PackageName);
	static FString GetDefaultPackageAssetPath(const FString& PackageName);
	
	static TSoftObjectPtr<UHoudiniToolsPackageAsset> GetPackageAssetRef(const FString& PackagePath);

	static FString GetAbsoluteToolsPackagePath(const UHoudiniToolsPackageAsset* ToolsPackage);

	static FString ResolveHoudiniAssetLabel(const UHoudiniAsset* HoudiniAsset);

	// Compute the file path of where we expect this HoudiniAsset's JSON file resides. 
	static bool GetHoudiniAssetJSONPath(const UHoudiniAsset* HoudiniAsset, FString& OutJSONFilePath);
	
	// Resolve the icon path for the given HoudiniAsset.
	static FString ResolveHoudiniAssetIconPath(const UHoudiniAsset* HoudiniAsset);

	// Get the path of the houdini asset relative to its owning package. 
	static bool ResolveHoudiniAssetRelativePath(const UHoudiniAsset* HoudiniAsset, FString& OutPath);

	static FAssetData GetAssetDataByObject(const UObject* AssetObject);
	
	// Check if the given directory is a valid HoudiniTools package.
	bool IsHoudiniToolsPackageDir(const FString& PackageBasePath) const;
	
	// Find all the Houdini Tools packages, in the Unreal project, where we should look for tools.
	void FindHoudiniToolsPackagePaths(TArray<FString>& HoudiniToolsDirectoryArray) const;

	static bool LoadJSONFile(const FString& JSONFilePath, TSharedPtr<FJsonObject>& OutJSONObject);
	static bool WriteJSONFile(const FString& JSONFilePath, const TSharedPtr<FJsonObject> JSONObject);

	static UHoudiniToolsPackageAsset* LoadHoudiniToolsPackage(const FString& PackageBasePath);

	// Add the given tool the exclusion list of the owning package for the given category.
	// If the package name was added to the Category exclusion list (or if it was already present), return true.
	// If the given category cannot be found in the package, return false.
	static bool ExcludeToolFromPackageCategory(UHoudiniAsset* HoudiniAsset, const FString& CategoryName, bool bAddCategoryIfMissing);

	void FindHoudiniToolsPackages(TArray<UHoudiniToolsPackageAsset*>& HoudiniToolPackages ) const;

	// From the current HoudiniAsset, search up the directory tree to see if we can find a
	// HoudiniToolsPackageAsset, since HoudiniAssets can live in subfolders in a package.
	// Returns the first (closest) HoudiniToolsPackageAsset found.
	static UHoudiniToolsPackageAsset* FindOwningToolsPackage(const UHoudiniAsset* HoudiniAsset);

	// Browse to the given UObject in the content browser.
	static void BrowseToObjectInContentBrowser(UObject* InObject);

	// Browse to the owning Tools Package in the content browser for the given Houdini Asset.
	static void BrowseToOwningPackageInContentBrowser(const UHoudiniAsset* HoudiniAsset);
	
	// Find all the HoudiniTools in the given tools package, in the Unreal project.
	// FHoudiniTools are cached for reuse.
	void FindHoudiniToolsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<TSharedPtr<FHoudiniTool>>& OutHoudiniTools);

	static void FindHoudiniAssetsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<UHoudiniAsset*>& OutAssets);

	// Apply the category rules to the given HoudiniTool. Append all valid categories for the HoudiniTool in OutCategories.  
	static void ApplyCategories(const TMap<FHoudiniToolCategory, FCategoryRules>& InCategories, const TSharedPtr<FHoudiniTool>& HoudiniTool, const bool bIgnoreExclusionRules, TArray<FString>& OutCategories);
	
	static void ApplyCategoryRules(
		const FString& HoudiniAssetRelPath,
		const FString& CategoryName,
		const TArray<FString>& IncludeRules,
		const TArray<FString>& ExcludeRules,
		const bool bIgnoreExclusionRules, 
		TArray<FString>& OutCategories
		);

	// Check whether the given name is a valid package name. If not, return the reason.
	static bool IsValidPackageName(const FString& PkgName, FText* OutFailReason);

	static bool CanCreateCreateToolsPackage(const FString& PackageName, FText* FailReason = nullptr);

	static UHoudiniToolsPackageAsset* CreateToolsPackageAsset(const FString& PackageDir, const FString& DefaultCategory, const FString& ExternalPackageDir=FString());

	// Return the UHoudiniToolData object on the HoudiniAsset. If it doesn't exist, create it. 
	static UHoudiniToolData* GetOrCreateHoudiniToolData(UHoudiniAsset* HoudiniAsset);

	// Populate FHoudiniTool from HoudiniAsset.
	void PopulateHoudiniTool(const TSharedPtr<FHoudiniTool>& HoudiniTool,
							 const UHoudiniAsset* InHoudiniAsset,
							 const UHoudiniToolsPackageAsset* InToolsPackage,
							 bool bIgnoreToolData = false);

	/**
	 * Rebuild the internal list of HoudiniTools
	 *
	 * This will refresh the tool list from the Unreal project. This will 
	 * not scan any external data. 
	 * **/
	void UpdateHoudiniToolListFromProject(bool bIgnoreExcludePatterns);

	// Get the cached categorized tools
	const TMap< FHoudiniToolCategory, TSharedPtr<FHoudiniToolList> >& GetCategorizedTools() const { return Categories; }

	// External Data

	// Read HDA description
	static bool GetHoudiniPackageDescriptionFromJSON(
		const FString& JsonFilePath,
		TMap<FString, FCategoryRules>& OutCategories,
		bool& OutImportPackageDescription, bool& OutExportPackageDescription, bool& OutImportToolsDescription, bool&
		OutExportToolsDescription
	);

	// Import external data for the given HoudiniAsset, if available
	static bool ImportExternalHoudiniAssetData(UHoudiniAsset* HoudiniAsset);
	
	// Reads the Houdini Tool Package Description from a JSON file
	static bool GetHoudiniToolDescriptionFromJSON(
		const FString& JsonFilePath,
		FString& OutName, EHoudiniToolType& OutType, EHoudiniToolSelectionType& OutSelectionType,
		FString& OutToolTip, FFilePath& OutIconPath, FFilePath& OutAssetPath, FString& OutHelpURL);

	// Import the Tools Package Asset only from the external json file into the given (internal) directory.
	// To import HDAs for this package, call ImportPackageHDA.  
	static UHoudiniToolsPackageAsset* ImportExternalToolsPackage(
		const FString& DestDir,
		const FString& PackageJsonPath,
		const bool bForceReimport);

	// Helper function that will try to import an external package from JSON, if it exists.
	// If not, simply create a default package in the Unreal project.
	static UHoudiniToolsPackageAsset* ImportOrCreateToolsPackage(
		const FString& DestDir,
		const FString& ExternalPackageDir,
		const FString& DefaultCategoryName);

	// Import the Tools Package Asset only from the external json file into the given (internal) directory.
	// To import HDAs for this package, call ImportPackageHDA.  
	static bool ReimportExternalToolsPackageDescription(UHoudiniToolsPackageAsset* PackageAsset);

	// TODO: Reimport all the HDAs for the given houdini asset package.
	// if bOnlyMissing == true, then only missing HDAs will be imported. Existing HDAs will be left untouched.
	static bool ReimportPackageHDAs(const UHoudiniToolsPackageAsset* PackageAsset, const bool bOnlyMissing=false, int* NumImportedHDAs=nullptr);

	/** 
	 * This will create a new unique slate brush from the HoudiniAsset's icon data, so be sure to keep a reference to this
	 * brush for as long as it is needed. 
	 * @param HoudiniTool The HoudiniTool for which the unique icon brush needs to be created. The brush will be
	 *                      created from the referenced UHoudiniAsset's ToolData.
	 * @returns nullptr if not able to create a dynamic brush.
	 */
	void CreateUniqueAssetIconBrush(TSharedPtr<FHoudiniTool> HoudiniTool);

	// Populate the HoudiniToolData from the given JSON file.
	static bool PopulateHoudiniToolDataFromJSON(const FString& JSONFilePath, UHoudiniToolData& OutToolData);
	
	static bool PopulateHoudiniToolsPackageFromJSON(const FString& JSONFilePath, UHoudiniToolsPackageAsset& OutPackage);

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

	// Convert between enums and string values used in JSON files
	static bool ToolTypeToString(const EHoudiniToolType ToolType, FString& OutString);
	static bool SelectionTypeToString(const EHoudiniToolSelectionType SelectionType, FString& OutString);

	// Attempt to import default SideFX HDAs from Houdini
	static bool ImportSideFXTools(int* NumImportedHDAs);

	// --------------------------------
	// Tools Package
	// --------------------------------

	static void PopulatePackageWithDefaultData(UHoudiniToolsPackageAsset* PackageAsset);

	// --------------------------------
	// User Categories
	// --------------------------------

	// Add the tool to the specified user category. If the category does not exist, it will be created.
	static void AddToolToUserCategory(const UHoudiniAsset* Asset, const FString& CategoryName);
	static void RemoveToolFromUserCategory(const UHoudiniAsset* Asset, const FString& CategoryName);

	// Get a list of user categories
	static void GetUserCategoriesList(TArray<FString>& OutCategories);

	// --------------------------------
	// Presets
	// --------------------------------

	static void CopySettingsToPreset(const UHoudiniAssetComponent* HAC, const bool bApplyAssetOptions, const bool bApplyBakeOptions, const bool
	                                 bApplyMeshGenSettings, const bool bApplyProxyMeshGenSettings, UHoudiniPreset* Preset);

	// Find all the presets that can be applied the given Houdini Asset. 
	static void FindPresetsForHoudiniAsset(const UHoudiniAsset* HoudiniAsset, TArray<UHoudiniPreset*>& OutPresets);

	// Apply the preset to the given HoudiniAssetComponent.
	// Optionally, reselect selected actors to update the component visualizers.
	static void ApplyPresetToHoudiniAssetComponent(
		const UHoudiniPreset* Preset,
		UHoudiniAssetComponent* HAC,
		const bool bReselectSelectedActors = true);

	// --------------------------------
	// Shutdown
	// --------------------------------

	// Be sure to call this method when the owning module shuts down so that we can remove
	// all our cached textures from root.
	void Shutdown();

protected:
	
	// Categories
	TMap< FHoudiniToolCategory, TSharedPtr<FHoudiniToolList> > Categories;

	TMap<FString, UTexture2D*> CachedTextures;
};
