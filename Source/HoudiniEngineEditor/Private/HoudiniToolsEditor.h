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

	DECLARE_MULTICAST_DELEGATE(FOnToolChanged)

	// Returns the Default Icon to be used by Houdini Tools
    static FString GetDefaultHoudiniToolIconPath();
	static FString GetDefaultHoudiniToolIconBrushName();

	static FString GetAbsoluteGameContentDir();

	static FString GetDefaultHoudiniToolsPath();
	static FString GetDefaultPackagePath(const FString& PackageName);
	static FString GetDefaultPackageAssetPath(const FString& PackageName);

	static FString GetAbsoluteToolsPackagePath(const UHoudiniToolsPackageAsset* ToolsPackage);

	static FString ResolveHoudiniAssetLabel(const UHoudiniAsset* HoudiniAsset);

	// Compute the file path of where we expect this HoudiniAsset's JSON file resides. 
	static bool GetHoudiniAssetJSONPath(const UHoudiniAsset* HoudiniAsset, FString& OutJSONFilePath);
	
	// Resolve the icon path for the given HoudiniAsset.
	static FString ResolveHoudiniAssetIconPath(const UHoudiniAsset* HoudiniAsset);

	// Resolve the path of the houdini asset relative to its owning package. 
	static bool ResolveHoudiniAssetRelativePath(const UHoudiniAsset* HoudiniAsset, FString& OutPath);
	
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
	static bool ExcludeToolFromCategory(UHoudiniAsset* HoudiniAsset, const FString& CategoryName, bool bAddCategoryIfMissing);

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
	void FindHoudiniToolsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<TSharedPtr<FHoudiniTool>>& OutHoudiniTools) const;

	static void FindHoudiniAssetsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<UHoudiniAsset*>& OutAssets);

	// Apply the category rules to the given HoudiniTool. Append all valid categories for the HoudiniTool in OutCategories.  
	static void ApplyCategories(const TMap<FString, FCategoryRules>& InCategories, const TSharedPtr<FHoudiniTool>& HoudiniTool, const bool bIgnoreExclusionRules, TArray<FString>& OutCategories);

	// Check whether the given name is a valid package name. If not, return the reason.
	static bool IsValidPackageName(const FString& PkgName, FText* OutFailReason);

	static bool CanCreateCreateToolsPackage(const FString& PackageName, FText* FailReason = nullptr);

	// Return the UHoudiniToolData object on the HoudiniAsset. If it doesn't exist, create it. 
	static UHoudiniToolData* GetOrCreateHoudiniToolData(UHoudiniAsset* HoudiniAsset);

	// Populate FHoudiniTool from HoudiniAsset.
    static void PopulateHoudiniTool(const TSharedPtr<FHoudiniTool>& HoudiniTool,
                             const UHoudiniAsset* InHoudiniAsset,
                             const UHoudiniToolsPackageAsset* InToolsPackage,
                             bool bIgnoreToolData = false);

	// Find the HoudiniTool that corresponds with the given HoudiniAsset.
	// Returns true if HoudiniAsset is valid, and FHoudiniTool was found. False otherwise. 
	bool FindHoudiniToolForAsset(UHoudiniAsset* HoudiniAsset, TSharedPtr<FHoudiniTool>& OutHoudiniTool);
	
	/**
	 * Rebuild the internal list of HoudiniTools
	 *
	 * This will refresh the tool list from the Unreal project. This will 
	 * not scan any external data. 
	 * **/
    void UpdateHoudiniToolListFromProject(bool bIgnoreExcludePatterns);

	/**
	 * If the package contains an external package source, generate json files for missing HDAs,
	 * reimport HDAs if necessary. 
	 **/
	void UpdatePackageToolListFromSource(UHoudiniToolsPackageAsset* ToolsPackage);

	// /** Return all the directories where we should look for houdini tools**/
 //    void GetAllHoudiniToolDirectories(TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray) const;
	// /** Return the directories where we should look for houdini tools**/
 //    void GetHoudiniToolDirectories(const int32& SelectedIndex, TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray) const;

	// TODO
	// Helper function to retrieve Package asset, given a UHoudiniAsset.
	// This will traverse up the directory tree from the Houdini Asset and return the first Package asset.
	//UHoudiniToolsPackageAsset* FindHoudiniToolsPackage(const UHoudiniAsset* HoudiniAsset);
	TArray< TSharedPtr<FHoudiniTool> >* GetHoudiniToolsForWrite() { return nullptr; }

	// Helper function for retrieving an HoudiniTool in the Editor list
    // bool FindHoudiniTool( const FHoudiniTool& Tool, int32& FoundIndex, bool& IsDefault );

	// DEPRECATED
	// Returns the HoudiniTools currently available for the shelf
    // const TArray< TSharedPtr<FHoudiniTool> >& GetHoudiniToolsList() const { return HoudiniTools; }

	const TMap< FString, TSharedPtr<FHoudiniToolList> >& GetCategorizedTools() const { return Categories; }

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

	// Import the Tools Package Asset only from the external json file into the given (internal) directory.
	// To import HDAs for this package, call ImportPackageHDA.  
	static bool ReimportExternalToolsPackageDescription(UHoudiniToolsPackageAsset* PackageAsset);

	// TODO: Reimport all the HDAs for the given houdini asset package.
	// if bOnlyMissing == true, then only missing HDAs will be imported. Existing HDAs will be left untouched.
	static bool ReimportPackageHDAs(const UHoudiniToolsPackageAsset* PackageAsset, const bool bOnlyMissing=false, int* NumImportedHDAs=nullptr);

	// TODO:
	// static bool ReimportExternalToolPackage() 
	
	// This will create a new unique slate brush from the HoudiniAsset's icon data, so be sure to keep a reference to this
	// brush for as long as it is needed.
	// Returns nullptr if not able to create a dynamic brush.
	static const FSlateBrush* CreateUniqueAssetIconBrush(const UHoudiniAsset* HoudiniAsset);

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

	/** Rebuild the editor's Houdini Tool list for a directory **/
    

    // /** Rebuild the editor's Houdini Tool list for a directory **/
    // void UpdateHoudiniToolList_DEPR(const FHoudiniToolDirectory& HoudiniToolsDirectory, const bool& isDefault );
	
    bool WriteJSONFromHoudiniTool_DEPR(const FHoudiniTool& Tool);

	// TODO: Store state in an external file in saved directory, not here
	//** Selected Houdini Tool Dir **/
    int32 CurrentHoudiniToolDirIndex;

	// TODO: TEMPORARY until Houdini Engine Shelf Tools have been migrated. This needs to be removed / replaced with individual shelf descriptors.
	// UPROPERTY(GlobalConfig, EditAnywhere, Category = CustomHoudiniTools)
	// mutable TArray<FHoudiniToolDirectory> CustomHoudiniToolsLocation;

	// Execute the OnToolChanged delegate.
	void BroadcastToolChanged();

	// One or more tools have changed / removed / added. 
	FOnToolChanged OnToolChanged;

protected:
	// Categories
	TMap< FString, TSharedPtr<FHoudiniToolList> > Categories;

	// We are currently maintaining this array too, for compatibility with some V1 functions,
	// until everything has been ported.
	TSet<TSharedPtr<FHoudiniTool>> HoudiniTools;
};
