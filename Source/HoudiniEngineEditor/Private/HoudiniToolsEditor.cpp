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

#include "HoudiniToolsEditor.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineEditorSettings.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterString.h"
#include "HoudiniPreset.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolsPackageAssetFactory.h"
#include "HoudiniToolsRuntimeUtils.h"
#include "HoudiniToolTypesEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetViewUtils.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameProjectUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "HoudiniParameterTranslator.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	#include "Subsystems/EditorAssetSubsystem.h"
#else
	#include "EditorAssetLibrary.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "TextureResource.h"
#endif

#ifdef LOCTEXT_NAMESPACE
// This undef is here to get rid of the definition from UE's DecoratedDragDropOp.h.
#undef LOCTEXT_NAMESPACE
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogHoudiniTools);

// Wrapper interface to manage code compatibility across multiple UE versions
struct FToolsWrapper
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	static FSoftObjectPath ConvertAssetObjectPath(const FString& Name) { return FSoftObjectPath(Name); }
#else
	static FName ConvertAssetObjectPath(const FString& Name) { return FName(Name); }
#endif

	static void CheckValidEditorAssetSubsystem()
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
		checkf(EditorAssetSubsystem != nullptr, TEXT("Invalid EditorAssetSubsystem"));
#else
		// Nothing to do here
#endif
	}

	static void ReimportAsync(UObject* Object, bool bAskForNewFileIfMissing, bool bShowNotification, const FString& PreferredReimportFile = TEXT(""))
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FReimportManager::Instance()->ReimportAsync(Object, bAskForNewFileIfMissing, bShowNotification, PreferredReimportFile);
#else
		FReimportManager::Instance()->Reimport(Object, bAskForNewFileIfMissing, bShowNotification, PreferredReimportFile);
#endif
	}

	static bool DoesDirectoryExist(const FString& DirectoryPath)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
		return EditorAssetSubsystem->DoesDirectoryExist(DirectoryPath);
#else
		return UEditorAssetLibrary::DoesDirectoryExist(DirectoryPath);
#endif
	}

	static bool MakeDirectory(const FString& DirectoryPath)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
		return EditorAssetSubsystem->DoesDirectoryExist(DirectoryPath);
#else
		return UEditorAssetLibrary::DoesDirectoryExist(DirectoryPath);
#endif
	}
};


IAssetRegistry&
FHoudiniToolsEditor::GetAssetRegistry()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
}


FHoudiniToolsEditor::FHoudiniToolsEditor()
{
	
}

FString
FHoudiniToolsEditor::GetDefaultHoudiniToolIconPath()
{
	return FHoudiniEngineEditor::Get().GetHoudiniEnginePluginDir() / TEXT("Content/Icons/icon_houdini_logo_40.png");
}

FString FHoudiniToolsEditor::GetDefaultHoudiniToolIconBrushName()
{
	return TEXT("HoudiniEngine.HoudiniEngineLogo40");
}

FString FHoudiniToolsEditor::GetAbsoluteGameContentDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
}


FString
FHoudiniToolsEditor::GetDefaultHoudiniToolsPath()
{
	return TEXT("/Game/HoudiniEngine/Tools/");
}

FString
FHoudiniToolsEditor::GetDefaultPackagePath(const FString& PackageName)
{
	FString BasePath = FHoudiniToolsEditor::GetDefaultHoudiniToolsPath(); 
	return UPackageTools::SanitizePackageName( FPaths::Combine(BasePath, PackageName) );
}

FString FHoudiniToolsEditor::GetDefaultPackageAssetPath(const FString& PackageName)
{
	const FString PkgBasePath = GetDefaultPackagePath(PackageName);
	const FString AssetPath = FPaths::Combine(PkgBasePath, FHoudiniToolsRuntimeUtils::GetPackageUAssetName());
	return AssetPath;
}

TSoftObjectPtr<UHoudiniToolsPackageAsset> FHoudiniToolsEditor::GetPackageAssetRef(const FString& PackagePath)
{
	const FString PackageName = FString::Format(TEXT("{0}.{0}"), { FHoudiniToolsRuntimeUtils::GetPackageUAssetName() } );
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	return TSoftObjectPtr<UHoudiniToolsPackageAsset>(FSoftObjectPath(FString::Format(TEXT("UHoudiniToolsPackageAsset'{0}/{1}'"), {PackagePath, PackageName})));
#else
	return TSoftObjectPtr<UHoudiniToolsPackageAsset>(FString::Format(TEXT("UHoudiniToolsPackageAsset'{0}/{1}'"), { PackagePath, PackageName }));
#endif
}

FString FHoudiniToolsEditor::GetAbsoluteToolsPackagePath(const UHoudiniToolsPackageAsset* ToolsPackage)
{
	if (!IsValid(ToolsPackage))
	{
		return FString();
	}
	if (ToolsPackage->ExternalPackageDir.Path.Len() == 0)
	{
		return FString();
	}
	
	// TODO: Can we normalize this path to get rid of the relative path bits?
	FString AbsPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()), ToolsPackage->ExternalPackageDir.Path);

	FPaths::NormalizeDirectoryName(AbsPath);
	FPaths::CollapseRelativeDirectories(AbsPath);
	AbsPath = FPaths::ConvertRelativePathToFull(AbsPath);

	return AbsPath;
}

FString FHoudiniToolsEditor::ResolveHoudiniAssetLabel(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
	{
		return FString();
	}

	if (IsValid(HoudiniAsset->HoudiniToolData))
	{
		return HoudiniAsset->HoudiniToolData->Name;
	}
	else
	{
		return FPaths::GetBaseFilename(HoudiniAsset->AssetFileName);
	}
}


bool
FHoudiniToolsEditor::GetHoudiniAssetJSONPath(const UHoudiniAsset* HoudiniAsset, FString& OutJSONFilePath)
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
FHoudiniToolsEditor::ResolveHoudiniAssetIconPath(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
	{
		return FString();
	}
	if (IsValid(HoudiniAsset->HoudiniToolData) && HoudiniAsset->HoudiniToolData->IconSourcePath.FilePath.Len() > 0)
	{
		// We have a custom icon path for this HoudiniAsset. 
		return HoudiniAsset->HoudiniToolData->IconSourcePath.FilePath;
	}

	// We couldn't find a valid icon path for this asset. Use the default path.
	return FHoudiniToolsRuntimeUtils::GetDefaultHoudiniAssetIconPath(HoudiniAsset);
}


bool
FHoudiniToolsEditor::ResolveHoudiniAssetRelativePath(const UObject* Object, FString& OutPath)
{
	if (!IsValid(Object))
		return false;

	const UHoudiniToolsPackageAsset* ToolsPackage = FindOwningToolsPackage(Object);
	if (!IsValid(ToolsPackage))
		return false;

	return ResolveHoudiniAssetRelativePath(Object, ToolsPackage, OutPath);
}


bool
FHoudiniToolsEditor::ResolveHoudiniAssetRelativePath(
	const UObject* AssetObject,
	const UHoudiniToolsPackageAsset* OwningPackageAsset,
	FString& OutPath)
{
	if (!IsValid(AssetObject))
		return false;
	
	if (!IsValid(OwningPackageAsset))
		return false;

	//NOTE: We're constructing a relative path here for the purposes of include/exclude pattern matching.
	//      to keep things as straightforward as possible, we always use the path name of the relevant
	//      asset for pattern matching. We will NOT be using the operator type or the tool label.
	
	const FString ToolsPackagePath = OwningPackageAsset->GetPackage()->GetPathName();

	const UPackage* Package = AssetObject->GetPackage();
	if (!IsValid(Package))
	{
		return false;
	}

	// Use the HoudiniAsset's relative (to the Package descriptor) path  for category matching.
	OutPath = Package->GetPathName();
	FPaths::MakePathRelativeTo(OutPath, *ToolsPackagePath);
	
	return true;
}


FAssetData FHoudiniToolsEditor::GetAssetDataByObject(const UObject* AssetObject)
{
	if (!IsValid(AssetObject))
	{
		return FAssetData();
	}
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath( FToolsWrapper::ConvertAssetObjectPath(AssetObject->GetPathName()) );
	
	if (!AssetData.IsInstanceOf( AssetObject->GetClass() ))
	{
		return FAssetData();
	}

	return AssetData;
}

bool FHoudiniToolsEditor::IsHoudiniToolsPackageDir(const FString& PackageBasePath) const
{
	const FString PkgPath = FPaths::Combine(PackageBasePath, FString::Format(TEXT("{0}.{0}"), { FHoudiniToolsRuntimeUtils::GetPackageUAssetName() }) );
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FToolsWrapper::ConvertAssetObjectPath(PkgPath));
	if (!AssetData.IsValid())
	{
		return false;
	}
	
	if (!AssetData.IsInstanceOf(UHoudiniToolsPackageAsset::StaticClass()))
	{
		return false;
	}
	return true;
}


void
FHoudiniToolsEditor::FindHoudiniToolsPackagePaths( TArray<FString>& HoudiniToolsDirectoryArray ) const
{
	// Scan the UE project to find valid HoudiniTools package directories.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	const TArray<FString>& SearchPaths = HoudiniRuntimeSettings->HoudiniToolsSearchPath;
	HoudiniToolsDirectoryArray.Empty();

	for(const FString& SearchPath : SearchPaths)
	{
		// Check if the current directory is a package directory
		// if not, then check each subdirectory for a houdini package.
		if (IsHoudiniToolsPackageDir(SearchPath))
		{
			HoudiniToolsDirectoryArray.Add(SearchPath);
			continue;
		}
		IAssetRegistry& AssetRegistry = GetAssetRegistry();

		TArray<FString> FoundFolders;
		AssetRegistry.EnumerateSubPaths(SearchPath, [&FoundFolders](const FString& Folder)
		{
			FoundFolders.Add(Folder);
			return true;
		},
		false);
		
		for (const FString& SubFolder : FoundFolders)
		{
			if (IsHoudiniToolsPackageDir(SubFolder))
			{
				HoudiniToolsDirectoryArray.Add(SubFolder);
			}
		}
	}
}

bool FHoudiniToolsEditor::LoadJSONFile(const FString& JSONFilePath, TSharedPtr<FJsonObject>& OutJSONObject)
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

bool FHoudiniToolsEditor::WriteJSONFile(const FString& JSONFilePath, const TSharedPtr<FJsonObject> JSONObject)
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

void FHoudiniToolsEditor::FindHoudiniToolsPackages(TArray<UHoudiniToolsPackageAsset*>& HoudiniToolPackages) const
{
	TArray<FString> PackagePaths;
	FindHoudiniToolsPackagePaths(PackagePaths);
	
	for (const FString& PackagePath : PackagePaths)
	{
		// Ingest tools from this package path
		UHoudiniToolsPackageAsset* PackageAsset = LoadHoudiniToolsPackage(PackagePath);
		if (!IsValid(PackageAsset))
		{
			continue;
		}
		HoudiniToolPackages.Add(PackageAsset);
	}
}

UHoudiniToolsPackageAsset*
FHoudiniToolsEditor::FindOwningToolsPackage(const UObject* Object)
{
	if (!IsValid(Object))
		return nullptr;

	// No need to load/find Tools package while cooking or running a commandlet
	// This would only generate unneeded warnings
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || GIsCookerLoadingPackage)
		return nullptr;

	FString CurrentPath = FPaths::GetPath(Object->GetPathName());

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

void FHoudiniToolsEditor::BrowseToObjectInContentBrowser(UObject* InObject)
{
	if (!IsValid(InObject))
		return;

	if (GEditor)
	{
		TArray<UObject *> Objects;
		Objects.Add(InObject);
		GEditor->SyncBrowserToObjects(Objects);
	}
}

void FHoudiniToolsEditor::BrowseToOwningPackageInContentBrowser(const UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
		return;

	UHoudiniToolsPackageAsset* ToolsPackage = FHoudiniToolsEditor::FindOwningToolsPackage(HoudiniAsset);
	BrowseToObjectInContentBrowser(ToolsPackage);
}

void FHoudiniToolsEditor::FindHoudiniToolsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<TSharedPtr<FHoudiniTool>>& OutHoudiniTools)
{
	OutHoudiniTools.Empty();
	if (!IsValid(ToolsPackage))
		return;

	const FString PackagePath = ToolsPackage->GetPathName();
	const FString PackageDir = FPaths::GetPath(PackagePath);
	
	const IAssetRegistry& AssetRegistry = GetAssetRegistry();

	TArray<FAssetData> AssetDataArray;
	AssetRegistry.GetAssetsByPath(FName(PackageDir), AssetDataArray, true, false);
	
	for (const FAssetData& AssetData : AssetDataArray)
	{
		if (!AssetData.IsInstanceOf(UHoudiniAsset::StaticClass()) &&
			!AssetData.IsInstanceOf(UHoudiniPreset::StaticClass()))
		{
			continue;
		}
		const UHoudiniPreset* HoudiniPreset = Cast<UHoudiniPreset>( AssetData.GetAsset() ); 
		const UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>( AssetData.GetAsset() );

		if (IsValid(HoudiniPreset))
		{
			HoudiniAsset = HoudiniPreset->SourceHoudiniAsset;
		}
		
		if (!IsValid(HoudiniAsset))
			continue;

		// Construct FHoudiniTool and populate from the current Houdini Asset and package.
		TSharedPtr<FHoudiniTool> HoudiniTool;
		
		// Create a new FHoudiniTool for this asset.
		HoudiniTool = MakeShareable( new FHoudiniTool() );

		PopulateHoudiniTool(HoudiniTool, HoudiniAsset, HoudiniPreset, ToolsPackage, false);
		OutHoudiniTools.Add(HoudiniTool);
	}
}

void FHoudiniToolsEditor::FindHoudiniAssetsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<UHoudiniAsset*>& OutAssets)
{
	OutAssets.Empty();
	if (!IsValid(ToolsPackage))
		return;

	const FString PackagePath = ToolsPackage->GetPathName();
	const FString PackageDir = FPaths::GetPath(PackagePath);
	
	const IAssetRegistry& AssetRegistry = GetAssetRegistry();

	TArray<FAssetData> AssetDataArray;
	AssetRegistry.GetAssetsByPath(FName(PackageDir), AssetDataArray, true, false);
	
	for (const FAssetData& AssetData : AssetDataArray)
	{
		if (!AssetData.IsInstanceOf(UHoudiniAsset::StaticClass()))
		{
			continue;
		}
		UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>( AssetData.GetAsset() );
		if (!IsValid(HoudiniAsset))
			continue;

		OutAssets.Add(HoudiniAsset);
	}
}

void
FHoudiniToolsEditor::ApplyCategories(
	const TMap<FHoudiniToolCategory, FCategoryRules>& InCategories,
	const TSharedPtr<FHoudiniTool>& HoudiniTool,
	const bool bIgnoreExclusionRules,
	TArray<FString>& OutIncludeCategories,
	TArray<FString>& OutExcludeCategories
	)
{
	if (!HoudiniTool.IsValid())
		return;
	
	const UObject* AssetObject = HoudiniTool->GetAssetObject();

	if (!IsValid(AssetObject))
		return;

	//NOTE: We're constructing a relative path here for the purposes of include/exclude pattern matching.
	//      to keep things as straightforward as possible, we always use the path name of the relevant
	//      asset for pattern matching. We will NOT use the operator type or the tool label.
	
	FString RelPath;
	if (!ResolveHoudiniAssetRelativePath(AssetObject, RelPath))
	{
		// if we can't resolve a relative path for this HoudiniAsset tool, we can't apply category matching.
		return;
	}

	for(const TTuple<FHoudiniToolCategory, FCategoryRules>& Entry : InCategories)
	{
		const FString& CategoryName = Entry.Key.Name;
		const FCategoryRules& Rules = Entry.Value;

		ApplyCategoryRules(RelPath, CategoryName, Rules.Include, Rules.Exclude, bIgnoreExclusionRules, OutIncludeCategories, OutExcludeCategories);
	}
}

void
FHoudiniToolsEditor::ApplyCategoryRules(
	const FString& HoudiniAssetRelPath,
	const FString& CategoryName,
	const TArray<FString>& IncludeRules,
	const TArray<FString>& ExcludeRules,
	const bool bIgnoreExclusionRules,
	TArray<FString>& OutIncludeCategories,
	TArray<FString>& OutExcludeCategories
	)
{
	if (!bIgnoreExclusionRules)
	{
		bool bExclude = false;
		
		// Apply exclude rules
		for (const FString& Pattern : ExcludeRules)
		{
			if (HoudiniAssetRelPath.MatchesWildcard(Pattern))
			{
				bExclude = true;
				break;
			}
		}
		if (bExclude)
		{
			// The HDA should be excluded from this category.
			OutExcludeCategories.Add(CategoryName);
			return;
		}
	}
	
	bool bExclude = false;
	
	// Apply exclude rules
	for (const FString& Pattern : ExcludeRules)
	{
		if (HoudiniAssetRelPath.MatchesWildcard(Pattern))
		{
			bExclude = true;
			break;
		}
	}
	if (bExclude)
	{
		// The HDA should be excluded from this category but will be included due 
		OutExcludeCategories.Add(CategoryName);
		
		if (bIgnoreExclusionRules == false)
		{
			return;
		}
	}

	// Apply include rules
	for (const FString& Pattern : IncludeRules)
	{
		if (HoudiniAssetRelPath.MatchesWildcard(Pattern))
		{
			OutIncludeCategories.Add(CategoryName);
			break;
		}
	}
}

bool
FHoudiniToolsEditor::IsValidPackageName(const FString& PkgName, FText* OutFailReason)
{
	// Code adapted from: GameProjectUtils::IsValidClassNameForCreation()
	if ( PkgName.IsEmpty() )
	{
		if (OutFailReason)
		{
			*OutFailReason = LOCTEXT( "HoudiniTools_NoPackageName", "You must specify a package name." );
		}
		return false;
	}

	if ( PkgName.Contains(TEXT(" ")) )
	{
		if (OutFailReason)
		{
			*OutFailReason = LOCTEXT( "HoudiniTools_PackageNameContainsSpace", "Your package name may not contain a space." );
		}
		return false;
	}

	if ( !FChar::IsAlpha(PkgName[0]) )
	{
		if (OutFailReason)
		{
			*OutFailReason = LOCTEXT( "HoudiniTools_PackageNameMustBeginWithACharacter", "Your package name must begin with an alphabetic character." );
		}
		return false;
	}

	FString IllegalNameCharacters;
	if ( !GameProjectUtils::NameContainsOnlyLegalCharacters(PkgName, IllegalNameCharacters) )
	{
		if (OutFailReason)
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("HoudiniTools_IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
			*OutFailReason = FText::Format( LOCTEXT( "HoudiniTools_PackageNameContainsIllegalCharacters", "The package name may not contain the following characters: '{IllegalNameCharacters}'" ), Args );
		}
		return false;
	}
	
	return true;
}

bool FHoudiniToolsEditor::CanCreateCreateToolsPackage(const FString& PackageName, FText* FailReason)
{
	if (!IsValidPackageName(PackageName, FailReason))
		return false;

	const FString AssetPath = GetDefaultPackageAssetPath(PackageName);
	
	if ( _DoesAssetExist(AssetPath) )
	{
		// Package already exists.
		if (FailReason)
		{
			*FailReason = LOCTEXT("PackageAlreadyExists", "Package already exists: '{0}'");
			*FailReason = FText::Format(*FailReason, {FText::FromString(AssetPath)});
		}
		return false;
	}

	return true;
}

UHoudiniToolsPackageAsset* FHoudiniToolsEditor::CreateToolsPackageAsset(
	const FString& PackageDir,
	const FString& DefaultCategory,
	const FString& ExternalPackageDir)
{
	UHoudiniToolsPackageAssetFactory* Factory = NewObject<UHoudiniToolsPackageAssetFactory>();
 
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	const FString PackageAssetName = FHoudiniToolsRuntimeUtils::GetPackageUAssetName(); // Name of the Houdini Tools Package asset.
	const FString FullPath = FPaths::Combine(PackageDir, PackageAssetName);
 
	UHoudiniToolsPackageAsset* Asset = Cast<UHoudiniToolsPackageAsset>(AssetToolsModule.Get().CreateAsset(
		PackageAssetName, PackageDir,
		UHoudiniToolsPackageAsset::StaticClass(), Factory, FName("ContentBrowserNewAsset")));
	 
	if (!Asset)
	{
		return nullptr;
	}

	// Clear out categories since new Houdini Tools Package assets have default category values.
	Asset->Categories.Empty();
	
	// By default add a catch-all category.
	FCategoryRules DefaultRule;
	DefaultRule.Include.Add("*");
	Asset->Categories.Add(DefaultCategory, DefaultRule);

	Asset->ExternalPackageDir.Path = ExternalPackageDir;
	FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(Asset, "ExternalPackageDir");

	return Asset;
}

UHoudiniToolData* FHoudiniToolsEditor::GetOrCreateHoudiniToolData(UHoudiniAsset* HoudiniAsset)
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

void
FHoudiniToolsEditor::PopulateHoudiniTool(const TSharedPtr<FHoudiniTool>& HoudiniTool,
								   const UHoudiniAsset* InHoudiniAsset,
								   const UHoudiniPreset* InHoudiniPreset,
								   const UHoudiniToolsPackageAsset* InToolsPackage,
								   bool bIgnoreToolData)
{
	if (!IsValid(InHoudiniAsset))
		return;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	HoudiniTool->HoudiniAsset = TSoftObjectPtr<UHoudiniAsset>(FSoftObjectPath(InHoudiniAsset->GetPathName()));
	HoudiniTool->ToolsPackage = TSoftObjectPtr<UHoudiniToolsPackageAsset>(FSoftObjectPath(InToolsPackage->GetPathName()));
#else
	HoudiniTool->HoudiniAsset = InHoudiniAsset;
	HoudiniTool->ToolsPackage = InToolsPackage;
#endif
	
	if (IsValid(InHoudiniPreset))
	{
		HoudiniTool->Name = FText::FromString(InHoudiniPreset->Name);
		HoudiniTool->ToolTipText = FText::FromString(InHoudiniPreset->Description);
	}
	else
	{
		if (!bIgnoreToolData && (IsValid(InHoudiniAsset->HoudiniToolData)))
		{
			// Populate from ToolData (this is our preferred data source) 
			UHoudiniToolData* ToolData = InHoudiniAsset->HoudiniToolData;
			
			HoudiniTool->Name = FText::FromString(ToolData->Name);
			HoudiniTool->ToolTipText = FText::FromString(ToolData->ToolTip);
		}
		else
		{
			// Populate from any data we can extract from UHoudiniAsset without looking outside the Unreal project.
			// i.e., no peeking at external json files.
			HoudiniTool->Name = FText::FromString(FPaths::GetBaseFilename(InHoudiniAsset->AssetFileName));
			HoudiniTool->ToolTipText = HoudiniTool->Name;
		}
	}
	
	if (!bIgnoreToolData && (IsValid(InHoudiniAsset->HoudiniToolData)))
	{
		// Populate from ToolData (this is our preferred data source) 
		UHoudiniToolData* ToolData = InHoudiniAsset->HoudiniToolData;
		
		HoudiniTool->HelpURL = ToolData->HelpURL;
		HoudiniTool->Type = ToolData->Type;
		HoudiniTool->SelectionType = ToolData->SelectionType;
	}
	else
	{
		HoudiniTool->HelpURL = FString();
		// We don't really know what this HDA is, so we're just going with "no inputs" as a default.
		HoudiniTool->Type = EHoudiniToolType::HTOOLTYPE_GENERATOR;
		HoudiniTool->SelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
	}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	HoudiniTool->HoudiniPreset = TSoftObjectPtr<UHoudiniPreset>(FSoftObjectPath(InHoudiniPreset->GetPathName()));
#else
	HoudiniTool->HoudiniPreset = InHoudiniPreset;
#endif
	HoudiniTool->PackageToolType = IsValid(InHoudiniPreset) ? EHoudiniPackageToolType::Preset : EHoudiniPackageToolType::HoudiniAsset;

	// if (IsValid(InHoudiniAsset->AssetImportData))
	// {
	// 	UAssetImportData* ImportData = InHoudiniAsset->AssetImportData;
	// 	if (ImportData->SourceData.SourceFiles.Num() > 0)
	// 	{
	// 		HoudiniTool->SourceAssetPath.FilePath = ImportData->SourceData.SourceFiles[0].RelativeFilename;
	// 	}
	// }

	CreateUniqueAssetIconBrush(HoudiniTool);
}

UHoudiniToolsPackageAsset* FHoudiniToolsEditor::LoadHoudiniToolsPackage(const FString& PackageBasePath)
{
	// No need to load/find Tools package while cooking or running a commandlet
	// This would only generate unneeded warnings
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || GIsCookerLoadingPackage)
		return nullptr;

	const FString PkgPath = FPaths::Combine(PackageBasePath, FString::Format(TEXT("{0}.{0}"), { FHoudiniToolsRuntimeUtils::GetPackageUAssetName() }) );
	return LoadObject<UHoudiniToolsPackageAsset>(nullptr, *PkgPath);	
}

bool FHoudiniToolsEditor::ExcludeToolFromPackageCategory(UObject* Object, const FString& CategoryName, bool bAddCategoryIfMissing)
{
	if (!IsValid(Object))
	{
		return false;
	}
	
	UHoudiniToolsPackageAsset* PackageAsset = FindOwningToolsPackage(Object);
	if (!IsValid(PackageAsset))
	{
		HOUDINI_LOG_WARNING(TEXT("[ExcludeToolFromCategory] Could not find owning package"));
		return false;
	}

	const bool bHasCategory = PackageAsset->Categories.Contains(CategoryName); 
	if (!bHasCategory && !bAddCategoryIfMissing)
	{
		// The category doesn't exist, and we don't want to add it.
		return false;
	}

	// Attempt to resolve the tool path relative to its owning package.
	FString RelToolPath;
	if (!ResolveHoudiniAssetRelativePath(Object, RelToolPath))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not resolve tool path relative to an owning package: %s."), *Object->GetPathName());
		return false;
	}

	FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniTools_ExcludeToolFromCategory", "Houdini Tools: Excluding tool from category"),
			PackageAsset);

	PackageAsset->Modify();

	// At this point, we either have the category, or we need to add it.
	// NOTE: We always perform category matching against the 'filename' of the asset
	FCategoryRules& CategoryRules = PackageAsset->Categories.FindOrAdd(CategoryName);
	if (!CategoryRules.Exclude.Contains(RelToolPath))
	{
		CategoryRules.Exclude.Add( RelToolPath );
	}

	PackageAsset->MarkPackageDirty();

	return true;
}


bool
FHoudiniToolsEditor::CanRemoveToolExcludeFromPackageCategory(UObject* Object, const FString& CategoryName)
{
	if (!IsValid(Object))
	{
		return false;
	}
	
	UHoudiniToolsPackageAsset* PackageAsset = FindOwningToolsPackage(Object);
	if (!IsValid(PackageAsset))
	{
		HOUDINI_LOG_WARNING(TEXT("[ExcludeToolFromCategory] Could not find owning package"));
		return false;
	}

	const bool bHasCategory = PackageAsset->Categories.Contains(CategoryName); 
	if (!bHasCategory)
	{
		// The category doesn't exist, and we don't want to add it.
		return false;
	}

	// Attempt to resolve the tool path relative to its owning package.
	FString RelToolPath;
	if (!ResolveHoudiniAssetRelativePath(Object, RelToolPath))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not resolve tool path relative to an owning package: %s."), *Object->GetPathName());
		return false;
	}

	// NOTE: We always perform exclude pattern matching against the relative path of the asset
	const FCategoryRules& CategoryRules = PackageAsset->Categories.FindChecked(CategoryName);
	return CategoryRules.Exclude.Contains(RelToolPath);
}


bool
FHoudiniToolsEditor::RemoveToolExcludeFromPackageCategory(UObject* Object, const FString& CategoryName)
{
	if (!IsValid(Object))
	{
		return false;
	}
	
	UHoudiniToolsPackageAsset* PackageAsset = FindOwningToolsPackage(Object);
	if (!IsValid(PackageAsset))
	{
		HOUDINI_LOG_WARNING(TEXT("[ExcludeToolFromCategory] Could not find owning package"));
		return false;
	}
	
	if (!PackageAsset->Categories.Contains(CategoryName))
	{
		// Category does not exist
		return false;
	}

	// Attempt to resolve the tool path relative to its owning package.
	FString RelToolPath;
	if (!ResolveHoudiniAssetRelativePath(Object, RelToolPath))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not resolve tool path relative to an owning package: %s."), *Object->GetPathName());
		return false;
	}

	FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniTools_ExcludeToolFromCategory", "Houdini Tools: Excluding tool from category"),
			PackageAsset);

	PackageAsset->Modify();

	// NOTE: We always perform exclude pattern matching against the relative path of the asset
	FCategoryRules& CategoryRules = PackageAsset->Categories.FindChecked(CategoryName);
	CategoryRules.Exclude.Remove( RelToolPath );

	PackageAsset->MarkPackageDirty();

	return true;
}


void
FHoudiniToolsEditor::UpdateHoudiniToolListFromProject(bool bIgnoreExcludePatterns)
{
	const UHoudiniEngineEditorSettings* HoudiniEngineEditorSettings = GetDefault<UHoudiniEngineEditorSettings>();

	Categories.Empty();

	TArray<UHoudiniToolsPackageAsset*> ToolsPackages;
	FindHoudiniToolsPackages(ToolsPackages);

	auto AddToolToCategoriesFn = [this](TSharedPtr<FHoudiniTool> HoudiniTool, const TArray<FString>& MatchingCategories)
	{
		for(const FString& CategoryName : MatchingCategories)
		{
			TSharedPtr<FHoudiniToolList>& CategoryTools = Categories.FindOrAdd(FHoudiniToolCategory(CategoryName, HoudiniTool->CategoryType));
			if (!CategoryTools.IsValid())
			{
				CategoryTools = MakeShared<FHoudiniToolList>();
			}

			CategoryTools->Tools.Add(HoudiniTool);
			CategoryTools->Packages.Add(HoudiniTool->ToolsPackage);
		}
	};

	auto AffirmToolCategoryFn = [this](const FString& CategoryName, const EHoudiniToolCategoryType CategoryType)
	{
		TSharedPtr<FHoudiniToolList>& ToolList = Categories.FindOrAdd(FHoudiniToolCategory(CategoryName, CategoryType));
		if (!ToolList.IsValid())
		{
			ToolList = MakeShared<FHoudiniToolList>();
		}
	};

	// Process categories defined in Tools Packages.
	for (const UHoudiniToolsPackageAsset* ToolsPackage : ToolsPackages)
	{
		if (!IsValid(ToolsPackage))
		{
			continue;
		}

		// Ensure that we have categories for each category defined in this package to allow us to display empty
		// categories.
		for (auto& Entry : ToolsPackage->Categories)
		{
			const FString& CategoryName = Entry.Key;
			AffirmToolCategoryFn(CategoryName, EHoudiniToolCategoryType::Package);
		}
		
		// Find all tools in this package
		TArray<TSharedPtr<FHoudiniTool>> PackageTools;
		FindHoudiniToolsInPackage(ToolsPackage, PackageTools);

		// Apply category rules to package tools
		for(TSharedPtr<FHoudiniTool> SharedHoudiniTool : PackageTools)
		{
			// We cannot use shared FHoudiniTools in list views, so we'll create unique copies for the items
			// to be used in slate views.
			TSharedPtr<FHoudiniTool> HoudiniTool = MakeShareable(new FHoudiniTool());
			*HoudiniTool = *SharedHoudiniTool;
			
			TMap<FHoudiniToolCategory, FCategoryRules> CategoryRules;
			for(auto& Entry : ToolsPackage->Categories)
			{
				CategoryRules.Add(FHoudiniToolCategory(Entry.Key, EHoudiniToolCategoryType::Package), Entry.Value);
			}

			// Find the matching categories from the package 
			TArray<FString> MatchingCategories, ExcludedCategories;
			ApplyCategories(CategoryRules, HoudiniTool, bIgnoreExcludePatterns, MatchingCategories, ExcludedCategories);
			HoudiniTool->CategoryType = EHoudiniToolCategoryType::Package;
			HoudiniTool->ExcludedFromCategories.Append(ExcludedCategories);

			// Add this tool to the respective category entries
			AddToolToCategoriesFn(HoudiniTool, MatchingCategories);
		}
	}
	
	// Process User defined categories
	if (HoudiniEngineEditorSettings)
	{
		const TMap<FString, FUserCategoryRules>& UserCategories = HoudiniEngineEditorSettings->UserToolCategories;
		for (auto& Entry : UserCategories)
		{
			const FString& CategoryName = Entry.Key;
			const FUserCategoryRules& Rules = Entry.Value;

			// Ensure that we have category entry to allow us to display empty categories
			AffirmToolCategoryFn(CategoryName, EHoudiniToolCategoryType::User);

			for(const FUserPackageRules& PackageRules : Rules.Packages)
			{
				// Find all tools in this package
				// TODO: Cache packages from previous for loop so that we don't have to do this work twice
				TArray<TSharedPtr<FHoudiniTool>> PackageTools;
				FindHoudiniToolsInPackage(PackageRules.ToolsPackageAsset, PackageTools);

				// Apply category rules to each package tool
				for(const TSharedPtr<FHoudiniTool>& SharedHoudiniTool : PackageTools)
				{
					TSharedPtr<FHoudiniTool> HoudiniTool = MakeShareable(new FHoudiniTool());
					*HoudiniTool = *SharedHoudiniTool;

					const  UObject* AssetObject = HoudiniTool->GetAssetObject();

					if (!IsValid(AssetObject))
					{
						continue;
					}

					//NOTE: We're constructing a relative path here for the purposes of include/exclude pattern matching.
					//      to keep things as straightforward as possible, we always use the path name of the relevant
					//      asset for pattern matching. We will NOT use the operator type or the tool label.
					
					FString HoudiniAssetRelPath;
					if (!ResolveHoudiniAssetRelativePath(AssetObject, HoudiniAssetRelPath))
					{
						// if we can't resolve a relative path for this HoudiniAsset tool, we can't apply category matching.
						continue;
					}

					TArray<FString> MatchingCategories, ExcludedCategories;
					ApplyCategoryRules(HoudiniAssetRelPath, CategoryName, PackageRules.Include, PackageRules.Exclude, bIgnoreExcludePatterns, MatchingCategories, ExcludedCategories);
					HoudiniTool->CategoryType = EHoudiniToolCategoryType::User;
					HoudiniTool->ExcludedFromCategories.Append(ExcludedCategories);
					
					// Add this tool to the respective category entries
					AddToolToCategoriesFn(HoudiniTool, MatchingCategories);
				}
			}
		}
	}

	// For now, ensure each category is sorted by HoudiniTool display name.
	// TODO: Implement a mechanism by which we can combine absolute order and wildcards/patterns (this might require
	//       sorting within confined subgroups)
	for(const TTuple<FHoudiniToolCategory, TSharedPtr<FHoudiniToolList>>& Entry : Categories)
	{
		const TSharedPtr<FHoudiniToolList> CategoryTools = Entry.Value;
		CategoryTools->Tools.Sort([](const TSharedPtr<FHoudiniTool>& LHS, const TSharedPtr<FHoudiniTool>& RHS) -> bool
		{
			return LHS->Name.ToString() < RHS->Name.ToString();
		});
	}
}

bool
FHoudiniToolsEditor::IsToolInCategory(
	const FHoudiniToolCategory& InCategory,
	TSharedPtr<FHoudiniTool> InHoudiniTool) const
{
	if (!Categories.Contains(InCategory))
	{
		return false;
	}

	const TSharedPtr<FHoudiniToolList> Tools = Categories.FindChecked(InCategory);
	for (const TSharedPtr<FHoudiniTool>& Tool : Tools->Tools)
	{
		if (Tool == InHoudiniTool)
		{
			return true;
		}
	}

	return false;
}


TSet<TSoftObjectPtr<UHoudiniToolsPackageAsset>>
FHoudiniToolsEditor::FindPackagesContainingCategory(const FString& InCategoryName) const
{
	for(const TTuple<FHoudiniToolCategory, TSharedPtr<FHoudiniToolList>>& Entry : Categories)
	{
		const FHoudiniToolCategory& Category = Entry.Key;
		const TSharedPtr<FHoudiniToolList> CategoryTools = Entry.Value;

		if (Category.Name == InCategoryName)
		{
			return CategoryTools->Packages;
		}
	}

	return {};
}


TSoftObjectPtr<UHoudiniToolsPackageAsset>
FHoudiniToolsEditor::FindFirstPackageContainingCategory(const FString& InCategoryName) const
{
	TSet<TSoftObjectPtr<UHoudiniToolsPackageAsset>> CategoryPackages;
	CategoryPackages = FindPackagesContainingCategory(InCategoryName);

	TArray<TSoftObjectPtr<UHoudiniToolsPackageAsset>> SortedPackages = CategoryPackages.Array();
	
	SortedPackages.Sort([](const TSoftObjectPtr<UHoudiniToolsPackageAsset>& LHS, const TSoftObjectPtr<UHoudiniToolsPackageAsset>& RHS) -> bool
	{
		return LHS->GetPathName() < RHS->GetPathName();
	});

	// Return the first valid Tools package asset.
	for (TSoftObjectPtr<UHoudiniToolsPackageAsset> PackagePtr : SortedPackages)
	{
		if (PackagePtr.IsValid())
		{
			return PackagePtr;
		}
	}

	return TSoftObjectPtr<UHoudiniToolsPackageAsset>();
}


bool FHoudiniToolsEditor::GetHoudiniPackageDescriptionFromJSON(const FString& JsonFilePath,
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

bool FHoudiniToolsEditor::ImportExternalHoudiniAssetData(UHoudiniAsset* HoudiniAsset)
{
	if (!IsValid(HoudiniAsset))
	{
		return false;
	}

	if (!IsValid(HoudiniAsset->AssetImportData))
	{
		return false;
	}

	// Get the expected path of the Houdini Asset's JSON file.
	FString JSONFilePath;
	if (!GetHoudiniAssetJSONPath(HoudiniAsset, JSONFilePath))
	{
		return false;
	}
	
	// If this HoudiniAsset has a matching JSON file, import it.
	bool bHasValidJSON = false;
	if (FPaths::FileExists(JSONFilePath))
	{
		bHasValidJSON = true;
	}

	bool bResult = false;

	if (bHasValidJSON)
	{
		// Ingest external JSON data and cache it on the HDA.
		// If a valid icon is referenced inside the JSON, it will overwrite the previously loaded icon.
		UHoudiniToolData* ToolData = GetOrCreateHoudiniToolData(HoudiniAsset);
		bResult = FHoudiniToolsEditor::PopulateHoudiniToolDataFromJSON(JSONFilePath, *ToolData);
	}
	
	if (!bResult)
	{
		// We don't have valid JSON data, so we'll populate ToolData with default data.
		UHoudiniToolData* ToolData = GetOrCreateHoudiniToolData(HoudiniAsset);
		ToolData->Name = FPaths::GetBaseFilename(HoudiniAsset->AssetFileName);
		ToolData->ToolTip = ToolData->Name;
		ToolData->Type = EHoudiniToolType::HTOOLTYPE_GENERATOR;
		ToolData->SelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
	}

	const FString IconPath = ResolveHoudiniAssetIconPath(HoudiniAsset);
	if (FPaths::FileExists(IconPath))
	{
		// If we have found an icon for this HDA, load it now.
		UHoudiniToolData* ToolData = GetOrCreateHoudiniToolData(HoudiniAsset);
		HoudiniAsset->Modify();
		HoudiniAsset->MarkPackageDirty();
		ToolData->LoadIconFromPath(IconPath);
	}
	else
	{
		// Could not find an icon. Clear any cached icon data. 
		UHoudiniToolData* ToolData = GetOrCreateHoudiniToolData(HoudiniAsset);
		ToolData->ClearCachedIcon();
	}
	
	return bResult;
}

bool
FHoudiniToolsEditor::GetHoudiniToolDescriptionFromJSON(const FString& JsonFilePath,
	FString& OutName, EHoudiniToolType& OutType, EHoudiniToolSelectionType& OutSelectionType,
	FString& OutToolTip, FFilePath& OutIconPath, FFilePath& OutAssetPath, FString& OutHelpURL )
{
	if ( JsonFilePath.IsEmpty() )
		return false;

	// Read the file as a string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString( FileContents, *JsonFilePath ) )
	{
		HOUDINI_LOG_ERROR( TEXT("Failed to import Houdini Tool .json file: %s"), *JsonFilePath);
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

	// First, check that the tool is compatible with Unreal
	bool IsCompatible = true;
	if (JSONObject->HasField(TEXT("target")))
	{
		IsCompatible = false;
		TArray<TSharedPtr<FJsonValue> >TargetArray = JSONObject->GetArrayField(TEXT("target"));
		for (TSharedPtr<FJsonValue> TargetValue : TargetArray)
		{
			if ( !TargetValue.IsValid() )
				continue;

			// Check the target array field contains either "all" or "unreal"
			FString TargetString = TargetValue->AsString();                        
			if ( TargetString.Equals( TEXT("all"), ESearchCase::IgnoreCase )
					|| TargetString.Equals(TEXT("unreal"), ESearchCase::IgnoreCase) )
			{
				IsCompatible = true;
				break;
			}
		}
	}

	if ( !IsCompatible )
	{
		// The tool is not compatible with unreal, skip it
		HOUDINI_LOG_MESSAGE( TEXT("Skipped Houdini Tool due to invalid target in JSON file: %s"), *JsonFilePath );
		return false;
	}

	// Then, we need to make sure the json file has a correponding hda
	// Start by looking at the assetPath field
	FString HDAFilePath = FString();
	if ( JSONObject->HasField( TEXT("assetPath") ) )
	{
		// If the json has the assetPath field, read it from there
		HDAFilePath = JSONObject->GetStringField(TEXT("assetPath"));
		if (!FPaths::FileExists(HDAFilePath))
			HDAFilePath = FString();
	}

	if (HDAFilePath.IsEmpty())
	{
		// Look for an hda file with the same name as the json file
		HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hda"));
		if (!FPaths::FileExists(HDAFilePath))
		{
			// Try .hdalc
			HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdalc"));
			if (!FPaths::FileExists(HDAFilePath))
			{
				// Try .hdanc
				HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdanc"));
				if (!FPaths::FileExists(HDAFilePath))
					HDAFilePath = FString();
			}
		}
	}

	if ( HDAFilePath.IsEmpty() )
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find the hda for the Houdini Tool .json file: %s"), *JsonFilePath);
		return false;
	}

	// Create a new asset.
	OutAssetPath = FFilePath{ HDAFilePath };

	// Read the tool name
	OutName = FPaths::GetBaseFilename(JsonFilePath);
	if ( JSONObject->HasField(TEXT("name") ) )
		OutName = JSONObject->GetStringField(TEXT("name"));

	// Read the tool type
	OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
	if ( JSONObject->HasField( TEXT("toolType") ) )
	{
		FString ToolType = JSONObject->GetStringField(TEXT("toolType"));
		if ( ToolType.Equals( TEXT("GENERATOR"), ESearchCase::IgnoreCase ) )
			OutType = EHoudiniToolType::HTOOLTYPE_GENERATOR;
		else if (ToolType.Equals( TEXT("OPERATOR_SINGLE"), ESearchCase::IgnoreCase ) )
			OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
		else if (ToolType.Equals(TEXT("OPERATOR_MULTI"), ESearchCase::IgnoreCase))
			OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI;
		else if (ToolType.Equals(TEXT("BATCH"), ESearchCase::IgnoreCase))
			OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH;
	}

	// Read the tooltip
	OutToolTip = FString();
	if ( JSONObject->HasField( TEXT("toolTip") ) )
	{
		OutToolTip =  JSONObject->GetStringField(TEXT("toolTip"));
	}

	// Read the help url
	OutHelpURL = FString();
	if (JSONObject->HasField(TEXT("helpURL")))
	{
		OutHelpURL = JSONObject->GetStringField(TEXT("helpURL"));
	}

	// Read the icon path
	FString IconPath = FString();
	if (JSONObject->HasField(TEXT("iconPath")))
	{
		// If the json has the iconPath field, read it from there
		IconPath = JSONObject->GetStringField(TEXT("iconPath"));
		if (!FPaths::FileExists(IconPath))
		{
			// Try to use this as a relative path and see if we can find a file
			IconPath = FPaths::Combine(FPaths::GetPath(JsonFilePath), IconPath);
			if (!FPaths::FileExists(IconPath))
			{
				// We still couldn't find anything. Giving up.
				IconPath = FString();
			}
		}
		
	}

	if (IconPath.IsEmpty())
	{
		// Look for a png file with the same name as the json file
		IconPath = JsonFilePath.Replace(TEXT(".json"), TEXT(".png"));
		if (!FPaths::FileExists(IconPath))
		{
			// We don't want to embed the default houdini tool icon. We only want to
			// embed custom icons. This allows the UI to change the default icon without
			// having to modify each asset.
			IconPath = FString();
		}
	}

	OutIconPath = FFilePath{ IconPath };

	// Read the selection types
	FString SelectionType = TEXT("All");
	if ( JSONObject->HasField(TEXT("UE_SelectionType")) )
		SelectionType = JSONObject->GetStringField( TEXT("UE_SelectionType") );

	if ( SelectionType.Equals( TEXT("CB"), ESearchCase::IgnoreCase ) )
		OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY;
	else if ( SelectionType.Equals( TEXT("World"), ESearchCase::IgnoreCase ) )
		OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY;
	else
		OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;

	return true;
}

UHoudiniToolsPackageAsset*
FHoudiniToolsEditor::ImportExternalToolsPackage(
	const FString& DestDir,
	const FString& PackageJsonPath,
	const bool bForceReimport)
{
	if (!FPaths::FileExists(PackageJsonPath))
	{
		HOUDINI_LOG_WARNING(TEXT("Unable to import tool package. JSON File does not exist: %s"), *PackageJsonPath);
		return nullptr;
	}


	if (!bForceReimport)
	{
		// If the destination directory doesn't contain any assets, we'll allow the import to take place.
		const FString PackageAssetPath = DestDir / FHoudiniToolsRuntimeUtils::GetPackageUAssetName();
		if (_DoesAssetExist(PackageAssetPath))
		{
			HOUDINI_LOG_WARNING(TEXT("Unable to import tool package. Asset already exists: %s"), *PackageAssetPath);
			return nullptr;
		}
	}

	// Import the package description json data
	
	TMap<FString, FCategoryRules> CategoryRules;
	bool bImportPackageDescription = false;
	bool bExportPackageDescription = false;
	bool bImportToolsDescription = false;
	bool bExportToolsDescription = false;
	if (!GetHoudiniPackageDescriptionFromJSON(
		PackageJsonPath,
		CategoryRules,
		bImportPackageDescription,
		bExportPackageDescription,
		bImportToolsDescription,
		bExportToolsDescription))
	{
		HOUDINI_LOG_WARNING(TEXT("Cannot load package description: %s"), *PackageJsonPath);
		return nullptr;
	}

	// Create the package 
	UHoudiniToolsPackageAssetFactory* Factory = NewObject<UHoudiniToolsPackageAssetFactory>();
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UHoudiniToolsPackageAsset* Asset = Cast<UHoudiniToolsPackageAsset>(AssetToolsModule.Get().CreateAsset(
		FHoudiniToolsRuntimeUtils::GetPackageUAssetName(), DestDir,
		UHoudiniToolsPackageAsset::StaticClass(), Factory, FName("ContentBrowserNewAsset")));

	if (!IsValid(Asset))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not create tools package: %s/%s"), *DestDir, *FHoudiniToolsRuntimeUtils::GetPackageUAssetName());
		return nullptr;
	}

	Asset->Modify();
	Asset->Categories = CategoryRules;
	Asset->bReimportPackageDescription = bImportPackageDescription;
	Asset->bExportPackageDescription = bExportPackageDescription;
	Asset->bReimportToolsDescription = bImportToolsDescription;
	Asset->bExportToolsDescription = bExportToolsDescription;
	
	Asset->ExternalPackageDir.Path = FPaths::GetPath(PackageJsonPath);
	
	FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(Asset, "ExternalPackageDir");

	return Asset;
}

UHoudiniToolsPackageAsset* FHoudiniToolsEditor::ImportOrCreateToolsPackage(
	const FString& DestDir,
	const FString& ExternalPackageDir,
	const FString& DefaultCategoryName)
{
	// If there is an external JSON file, import it. Otherwise, create a new package.

	UHoudiniToolsPackageAsset* PackageAsset = nullptr;

	const FString JSONFilePath = FPaths::Combine(DestDir, FHoudiniToolsRuntimeUtils::GetPackageJSONName());
	if (FPaths::FileExists(JSONFilePath))
	{
		// Try to import the external package.
		PackageAsset = ImportExternalToolsPackage(DestDir, JSONFilePath, true);
	}

	if (!IsValid(PackageAsset))
	{
		// Create a new, default, package and configure it to point to the external package dir
		PackageAsset = FHoudiniToolsEditor::CreateToolsPackageAsset(
			DestDir,
			DefaultCategoryName,
			ExternalPackageDir
			);
	}

	return PackageAsset;
}

bool
FHoudiniToolsEditor::ReimportExternalToolsPackageDescription(UHoudiniToolsPackageAsset* PackageAsset)
{
	if (!IsValid(PackageAsset))
	{
		return false;
	}
	
	const FString JSONPath = FHoudiniToolsRuntimeUtils::GetHoudiniToolsPackageJSONPath(PackageAsset);

	if (!PackageAsset->bReimportPackageDescription)
	{
		// This package does not permit package description reimports. Stop now.
		HOUDINI_LOG_WARNING(TEXT("bReimportPackageDescription is false for Tools package '%s'. Skipping"), *PackageAsset->GetPathName());
		return false;
	}
	
	return PopulateHoudiniToolsPackageFromJSON(JSONPath, *PackageAsset);
}

bool
FHoudiniToolsEditor::ReimportPackageHDAs(const UHoudiniToolsPackageAsset* PackageAsset, const bool bOnlyMissing, int* NumImportedHDAs)
{
	if (!IsValid(PackageAsset))
	{
		return false;
	}

	const FString ExternalPackageDir = GetAbsoluteToolsPackagePath(PackageAsset) + TEXT("/");
	if (!FPaths::DirectoryExists(ExternalPackageDir))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find external package dir: %s"), *ExternalPackageDir);
		return false;
	}

	FToolsWrapper::CheckValidEditorAssetSubsystem();

	// First, collect all the internal HDAs present in this package.
	// We'll be removing entries from this array while reimporting any external HDAs.
	// If anything is left over in this array, it means they have a different external
	// source and will have to be reimported manually.
	TArray<UHoudiniAsset*> InternalHoudiniAssets;
	FindHoudiniAssetsInPackage(PackageAsset, InternalHoudiniAssets);

	// Find all HDAs in the external package directory
	const FString PackageAssetDir = FPaths::GetPath( PackageAsset->GetPathName() );

	TArray<FString> PackageFiles;
	IFileManager::Get().FindFilesRecursive(PackageFiles, *ExternalPackageDir, TEXT("*"), true, false);


	for(const FString& ExternalHDAFile : PackageFiles)
	{
		if (ExternalHDAFile.MatchesWildcard("*/backup/*"))
		{
			// Ignore anything in a backup directory.
			continue;
		}
		
		// Check whether this is an .hda, .hdalc, .hdanc file:
		const FString Extension = FPaths::GetExtension(ExternalHDAFile);
		if (!(Extension == TEXT("hda") || Extension == TEXT("hdalc") || Extension == TEXT("hdanc")))
		{
			// this is not the hda we're looking for.
			continue;
		}

		// NOTE the HDAFile paths are absolute. Convert them to relative and start checking whether we can import them.
		FString RelExternalHDAFile = ExternalHDAFile;
		FPaths::MakePathRelativeTo(RelExternalHDAFile, *ExternalPackageDir);
		
		if (NumImportedHDAs)
		{
			(*NumImportedHDAs)++;
		}

		// Check if the HDA already exists on the game project
		const FString ToolName = ObjectTools::SanitizeObjectName( FPaths::GetBaseFilename( RelExternalHDAFile ) );
		const FString RelToolPath = ObjectTools::SanitizeObjectPath( FPaths::GetPath(RelExternalHDAFile) );
		const FString ToolDir = FPaths::Combine(PackageAssetDir, RelToolPath);
		const FString ToolPath = FPaths::Combine(ToolDir, ToolName);
		const FString ToolAssetRefPath = FString::Printf(TEXT("HoudiniAsset'%s.%s'"), *ToolPath, *ToolName);

		// We are now ready to import the HDA.
		// If it doesn't exist, create a new UHoudiniAsset. If it already exists, reimport it using the above ToolPath.

		// Construct the asset's ref
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		TSoftObjectPtr<UHoudiniAsset> HoudiniAssetRef = TSoftObjectPtr<UHoudiniAsset>(FSoftObjectPath(ToolAssetRefPath));
#else
		TSoftObjectPtr<UHoudiniAsset> HoudiniAssetRef(ToolAssetRefPath);
#endif
		UHoudiniAsset* LoadedAsset = nullptr;

		//
		// See if the HDA needs to be imported in Unreal, or just loaded
		bool bHDAExists = false;
		if ( !HoudiniAssetRef.IsValid() )
		{
			if (_DoesAssetExist(ToolPath))
			{
				// Try to load the asset, if it exists
				LoadedAsset = HoudiniAssetRef.LoadSynchronous();
				if ( IsValid(LoadedAsset) )
				{
					bHDAExists = HoudiniAssetRef.IsValid();
				}
			}
		}
		else
		{
			bHDAExists = true;
			LoadedAsset = HoudiniAssetRef.LoadSynchronous();
		}

		if (bHDAExists && bOnlyMissing)
		{
			// We're only interesting in missing HDAs
			continue;
		}

		if (!bHDAExists)
		{
			// We check an edge case here where an asset might exist at the toolpath, but it's not an HoudiniAsset.
			if (_DoesAssetExist(ToolPath))
			{
				//  Scatter/prox/va.he_proximity_scatter.1.0.hda -> /Game/HoudiniEngine/Tools/Scatter/va_he_proximity_scatter_1_0
				HOUDINI_LOG_WARNING(TEXT("Cannot not reimport existing HoudiniAsset due to invalid type: %s"), *ToolPath);
				continue;
			}
		}

		// If we have reached this point, the HDA needs to be imported or created.

		if (bHDAExists)
		{
			// Reimport existing asset from the current tool path

			InternalHoudiniAssets.Remove(LoadedAsset);
			FToolsWrapper::ReimportAsync(LoadedAsset, false, true, ExternalHDAFile);
		}
		else
		{
			// Create / import a new one
			// Automatically import the HDA and create a uasset for it
			FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
			TArray<FString> FileNames;
			FileNames.Add(ExternalHDAFile);
			
			if ( !FToolsWrapper::DoesDirectoryExist(ToolDir) )
			{
				FToolsWrapper::MakeDirectory(ToolDir);
			}

			UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
			ImportData->bReplaceExisting = true;
			ImportData->bSkipReadOnly = true;
			ImportData->Filenames = FileNames;
			ImportData->DestinationPath = ToolDir;
			ImportData->FactoryName = UHoudiniAssetFactory::StaticClass()->GetName();
			TArray<UObject*> AssetArray = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
			if ( AssetArray.Num() <=  0 )
				continue;
		
			HoudiniAssetRef = Cast< UHoudiniAsset >(AssetArray[0]);
			if (!HoudiniAssetRef.IsValid() )
				continue;
		
			// // Try to save the newly created package
			// UPackage* Pckg = Cast<UPackage>( HoudiniAssetRef->GetOuter() );
			// if (IsValid(Pckg) && Pckg->IsDirty() )
			// {
			//     Pckg->FullyLoad();
			//     UPackage::SavePackage(
			//         Pckg, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
			//         *FPackageName::LongPackageNameToFilename( Pckg->GetName(), FPackageName::GetAssetPackageExtension() ) );
			// }
			HoudiniAssetRef->MarkPackageDirty();
		}

	}
	
	// reimport any HDAs left over in this array 
	for (UHoudiniAsset* HoudiniAsset : InternalHoudiniAssets)
	{
		FToolsWrapper::ReimportAsync(HoudiniAsset, false, true);
	}
	
	return false;
}

void
FHoudiniToolsEditor::CreateUniqueAssetIconBrush(TSharedPtr<FHoudiniTool> HoudiniTool)
{
	if (!HoudiniTool.IsValid())
	{
		return;
	}

	FString ResourceName;
	bool bHasSourceImage = false;
	FImage SrcImage;
	FString RawDataMD5;
	
	UHoudiniPreset* HoudiniPreset = HoudiniTool->HoudiniPreset.LoadSynchronous();
	if (IsValid(HoudiniPreset))
	{
		// We have a preset we must try to use this icon. Don't fall back to the SourceAsset icon. We want the
		// icon (and preset thumbnail) to be managed independently from the source asset, otherwise things can get
		// visually confusing in the frontend.

		const FHImageData& SrcImageData = HoudiniPreset->IconImageData;

		if (SrcImageData.SizeX == 0 || SrcImageData.SizeY == 0 || SrcImageData.RawData.Num() == 0)
		{
			return;
		}

		if (SrcImageData.Format != ERawImageFormat::BGRA8)
		{
			HOUDINI_LOG_WARNING(TEXT("Cannot create unique icon brush. Requires raw image format BGRA8."));
			return;
		}

		// Copy cached image data to SrcImage
		SrcImageData.ToImage(SrcImage);
		RawDataMD5 = SrcImageData.RawDataMD5;
		bHasSourceImage = true;
		ResourceName = UObject::RemoveClassPrefix( *HoudiniPreset->GetPathName() );
	}
	else
	{
		// We dont have a valid source image yet. Try to get one from the Houdini Asset.
		const UHoudiniAsset* HoudiniAsset = HoudiniTool->HoudiniAsset.LoadSynchronous();
		if (IsValid(HoudiniAsset) && IsValid(HoudiniAsset->HoudiniToolData))
		{
			// We have a valid HoudiniAsset in the tool data. Let's try to create an icon brush.
			
			const FHImageData& SrcImageData = HoudiniAsset->HoudiniToolData->IconImageData;

			if (SrcImageData.SizeX == 0 || SrcImageData.SizeY == 0 || SrcImageData.RawData.Num() == 0)
			{
				return;
			}

			if (SrcImageData.Format != ERawImageFormat::BGRA8)
			{
				HOUDINI_LOG_WARNING(TEXT("Cannot create unique icon brush. Requires raw image format BGRA8."));
				return;
			}

			// Copy cached image data to SrcImage
			SrcImageData.ToImage(SrcImage);
			RawDataMD5 = SrcImageData.RawDataMD5;
			bHasSourceImage = true;
			ResourceName = UObject::RemoveClassPrefix( *HoudiniAsset->GetPathName() );
		}
	}

	if (bHasSourceImage)
	{
		
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		if (!SrcImage.IsImageInfoValid())
		{
			return;
		}
#endif
		
		

		
		// Append the hash of the image data. This will allow the icon to create a unique resource
		// for different image data, even if the icon image path is the same. At the same time, we're
		// able to reuse image resource that haven't changed.
		const FString UniqueResourceName = FString::Format(TEXT("{0}_{1}"), {ResourceName, RawDataMD5});
		const FName UniqueResourceFName(UniqueResourceName);
		
		
		if (HoudiniTool->Icon.IsValid())
		{
			// We have an existing slate brush. We'll reuse it.
			if ( HoudiniTool->Icon->GetResourceName().Compare(FName(UniqueResourceName)) == 0 )
			{
				return;
			}
		}
		
		const FString TextureName = FString::Format(TEXT("{0}_{1}"), {FPaths::GetBaseFilename(ResourceName), RawDataMD5});

		UTexture2D* IconTexture = nullptr;

		if (CachedTextures.Contains(RawDataMD5))
		{
			// Reuse the previously cached texture
			IconTexture = CachedTextures.FindChecked(RawDataMD5);
		}

		if (!IsValid(IconTexture))
		{
			// Create a new texture and cache it
			IconTexture = UTexture2D::CreateTransient(SrcImage.SizeX, SrcImage.SizeY, PF_B8G8R8A8, FName(TextureName));
			if ( IconTexture )
			{
				IconTexture->bNotOfflineProcessed = true;
				uint8* MipData = static_cast< uint8* >( IconTexture->GetPlatformData()->Mips[ 0 ].BulkData.Lock( LOCK_READ_WRITE ) );
				
				// Bulk data was already allocated for the correct size when we called CreateTransient above
				FMemory::Memcpy( MipData, &SrcImage.RawData[0], IconTexture->GetPlatformData()->Mips[ 0 ].BulkData.GetBulkDataSize() );
				
				IconTexture->GetPlatformData()->Mips[ 0 ].BulkData.Unlock();
				IconTexture->UpdateResource();

				// Set compression options.
				IconTexture->SRGB = true;
				IconTexture->CompressionSettings = TC_EditorIcon;
				IconTexture->MipGenSettings = TMGS_FromTextureGroup;
				IconTexture->DeferCompression = true;
				IconTexture->PostEditChange();

				// Cache the texture so that we can reuse it
				CachedTextures.Add(RawDataMD5, IconTexture);
				IconTexture->AddToRoot();
				HoudiniTool->IconTexture = IconTexture;
			}
		}

		// Create a new brush, using the current IconTexture.
		const TSharedPtr<FSlateBrush> Icon = MakeShareable( new FSlateDynamicImageBrush(
			IconTexture,
			FVector2D(SrcImage.SizeX, SrcImage.SizeY),
			UniqueResourceFName
			));
		
		HoudiniTool->Icon = Icon;
		return;
	}

	HoudiniTool->Icon = nullptr;
	HoudiniTool->IconTexture = nullptr;
}


void
FHoudiniToolsEditor::CaptureThumbnailFromViewport(UObject* Asset)
{
	// Capture thumbnail for the preset from the viewport, and use it as the preset icon.
	FViewport* Viewport = GEditor->GetActiveViewport();
	if ( ensure(GCurrentLevelEditingViewportClient) && ensure(Viewport) )
	{
		//have to re-render the requested viewport
		FLevelEditorViewportClient* OldViewportClient = GCurrentLevelEditingViewportClient;
		//remove selection box around client during render
		GCurrentLevelEditingViewportClient = NULL;
		Viewport->Draw();
	
	
		TArray<FAssetData> AssetDataArray;
		AssetDataArray.Add( FHoudiniToolsEditor::GetAssetDataByObject(Asset) );
		AssetViewUtils::CaptureThumbnailFromViewport(Viewport, AssetDataArray);
	
		//redraw viewport to have the yellow highlight again
		GCurrentLevelEditingViewportClient = OldViewportClient;
		Viewport->Draw();
	}
}


bool
FHoudiniToolsEditor::CopyThumbnailToImage(const UObject* Asset, FHImageData& OutImageData)
{
	FObjectThumbnail Thumbnail;
	if (ThumbnailTools::AssetHasCustomThumbnail(Asset->GetFullName(), Thumbnail))
	{
		TArray<uint8> ImageData = Thumbnail.GetUncompressedImageData();
		OutImageData.RawData = MoveTemp(ImageData);
		OutImageData.SizeX = Thumbnail.GetImageWidth();
		OutImageData.SizeY = Thumbnail.GetImageHeight();
		OutImageData.Format = ERawImageFormat::BGRA8;
		return true;
	}
	return false;
}


bool
FHoudiniToolsEditor::PopulateHoudiniToolDataFromJSON(const FString& JSONFilePath, UHoudiniToolData& OutToolData)
{
	OutToolData.Modify();

	// Attempt to extract the Tool info from the JSON file
	return GetHoudiniToolDescriptionFromJSON(
		JSONFilePath,
		OutToolData.Name,
		OutToolData.Type,
		OutToolData.SelectionType,
		OutToolData.ToolTip,
		OutToolData.IconSourcePath,
		OutToolData.SourceAssetPath,
		OutToolData.HelpURL);
}

bool FHoudiniToolsEditor::PopulateHoudiniToolsPackageFromJSON(const FString& JSONFilePath, UHoudiniToolsPackageAsset& OutPackage)
{
	OutPackage.Modify();

	const bool bResult = GetHoudiniPackageDescriptionFromJSON(
		JSONFilePath,
		OutPackage.Categories,
		OutPackage.bReimportPackageDescription,
		OutPackage.bExportPackageDescription,
		OutPackage.bReimportToolsDescription,
		OutPackage.bExportToolsDescription);

	OutPackage.PostEditChange();
	OutPackage.MarkPackageDirty();
	return bResult;
}

TSharedPtr<FJsonObject> FHoudiniToolsEditor::MakeDefaultJSONObject(const UHoudiniAsset* HoudiniAsset)
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
	FHoudiniToolsEditor::SelectionTypeToString(SelectionType, SelectionTypeString);
	JSONObject->SetStringField(TEXT("UE_SelectionType"), SelectionTypeString);

	return JSONObject;
}

TSharedPtr<FJsonObject> FHoudiniToolsEditor::MakeDefaultJSONObject(const UHoudiniToolsPackageAsset* ToolsPackage)
{
	if (!IsValid(ToolsPackage))
		return nullptr;

	// A default JSON file for a tools package will contain a single category using the directory
	// name of the ToolsPackage.
	// All reimport/export settings will be disabled by default.

	const FString AssetPath = FPaths::GetPath(ToolsPackage->GetPathName());
	const FString PackageName = FPaths::GetBaseFilename(AssetPath);
	
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

bool FHoudiniToolsEditor::WriteDefaultJSONFile(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath)
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

bool FHoudiniToolsEditor::WriteDefaultJSONFile(const UHoudiniToolsPackageAsset* ToolsPackage, const FString& JSONFilePath)
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

bool FHoudiniToolsEditor::WriteJSONFromHoudiniAsset(const UHoudiniAsset* HoudiniAsset, const FString& JSONFilePath)
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
			// We were unable to parse it, so we'll fail here. We don't want to override
			// files if we can't preserve its contents.
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
	if ( !IconSourcePath.IsEmpty() )
	{
		JSONObject->SetStringField(TEXT("iconPath"), IconSourcePath);
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

bool FHoudiniToolsEditor::WriteJSONFromToolPackageAsset(const UHoudiniToolsPackageAsset* ToolsPackage,
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

bool FHoudiniToolsEditor::ToolTypeToString(const EHoudiniToolType ToolType, FString& OutString)
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

bool FHoudiniToolsEditor::SelectionTypeToString(const EHoudiniToolSelectionType SelectionType, FString& OutString)
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

bool FHoudiniToolsEditor::ImportSideFXTools(int* NumImportedHDAs)
{
	// Read the default tools from the $HFS/engine/tool folder
	FDirectoryPath DefaultToolPath;
	const FString HFSPath = FPaths::GetPath(FHoudiniEngine::Get().GetLibHAPILocation());
	const FString ExternalPackagePath = FPaths::Combine(HFSPath, TEXT("engine"), TEXT("tools"));
	const FString PackageJSONPath = FPaths::Combine(ExternalPackagePath, FHoudiniToolsRuntimeUtils::GetPackageJSONName());

	// Base Path of the tools directory in the Unreal project.
	const FString PackageName = TEXT("SideFX");
	const FString PackageDestPath = FHoudiniToolsEditor::GetDefaultPackagePath(PackageName);

	// Path to the Tools Package asset contained in the PackageDestPath.
	const FString PackageAssetPath = FHoudiniToolsEditor::GetDefaultPackageAssetPath(PackageName);
	UHoudiniToolsPackageAsset* PackageAsset = nullptr;

	// See if there is an existing SideFX package that we can work with.
	if (_DoesAssetExist(PackageAssetPath))
	{
		// Try to load the asset
		const TSoftObjectPtr<UHoudiniToolsPackageAsset> SideFXPackageRef = GetPackageAssetRef(PackageDestPath);
		PackageAsset = SideFXPackageRef.LoadSynchronous();

		if (IsValid(PackageAsset))
		{
			PackageAsset->Modify();

			// We have a valid existing package. Be sure to update the package path
			PackageAsset->ExternalPackageDir.Path = ExternalPackagePath;
			FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(PackageAsset, "ExternalPackageDir");
			
			PackageAsset->MarkPackageDirty();
		}
	}
	
	// Regardless of whether we have found an existing SideFX package in the Unreal project, we'll reimport the
	// external package.
	PackageAsset = FHoudiniToolsEditor::ImportOrCreateToolsPackage(
		PackageDestPath,
		ExternalPackagePath,
		PackageName
		);
	
	if (!PackageAsset)
	{
		// Could not import / create a package
		HOUDINI_LOG_WARNING(TEXT("Error importing SideFX tools. Could not import or create HoudiniTools package."));
		return false;
	}
	
	// Reimport HDAs for the current package.
	FHoudiniToolsEditor::ReimportPackageHDAs(PackageAsset, false, NumImportedHDAs);

	return true;
}

void FHoudiniToolsEditor::PopulatePackageWithDefaultData(UHoudiniToolsPackageAsset* PackageAsset)
{
	if (!IsValid(PackageAsset))
	{
		return;
	}
	
	FCategoryRules DefaultRules;
	DefaultRules.Include.Add("*");

	PackageAsset->Modify();
	
	PackageAsset->Categories.Empty();
	PackageAsset->Categories.Add("Default Package Category", DefaultRules);
}


FText
FHoudiniToolsEditor::GetFavoritesCategoryName()
{
	return NSLOCTEXT("HoudiniEngine", "HoudiniTools_FavoritesCategoryName", "Favorites");
}


void
FHoudiniToolsEditor::AddToolToUserCategory(const UObject* Object, const FString& CategoryName)
{
	if (!IsValid(Object))
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot add tool to category. Invalid asset."));
		return;
	}
		
	UHoudiniToolsPackageAsset* Package = FHoudiniToolsEditor::FindOwningToolsPackage(Object);
	if (!IsValid(Package))
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot add tool to category. Invalid owning package"));
		return;
	}

	UHoudiniEngineEditorSettings* Settings = GetMutableDefault<UHoudiniEngineEditorSettings>();
	if (!Settings)
	{
		return;
	}

	FUserCategoryRules& Rules = Settings->UserToolCategories.FindOrAdd(CategoryName); 

	// Try to find an entry containing the owning package.
	int FoundIndex = INDEX_NONE;
	for (int i = 0; i < Rules.Packages.Num(); ++i)
	{
		if (Rules.Packages[i].ToolsPackageAsset == Package)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// This package does not yet have an entry in this category. Create one now.
		FoundIndex = Rules.Packages.Add(FUserPackageRules());
		Rules.Packages[FoundIndex].ToolsPackageAsset = Package;
	}

	FUserPackageRules& PackageRules = Rules.Packages[FoundIndex];

	// Get the relative tool path and add it to the package rules.
	FString RelPath;
	ResolveHoudiniAssetRelativePath(Object, RelPath);
	
	PackageRules.Include.AddUnique(RelPath);
	FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(Settings, "UserToolCategories");
	
	Settings->SaveConfig();
}

void FHoudiniToolsEditor::RemoveToolFromUserCategory(const UObject* Object, const FString& CategoryName)
{
	if (!IsValid(Object))
	{
		return;
	}
		
	UHoudiniToolsPackageAsset* Package = FHoudiniToolsEditor::FindOwningToolsPackage(Object);
	if (!IsValid(Package))
	{
		return;
	}

	UHoudiniEngineEditorSettings* Settings = GetMutableDefault<UHoudiniEngineEditorSettings>();
	if (!Settings)
	{
		return;
	}

	if (!Settings->UserToolCategories.Contains(CategoryName))
	{
		return;
	}

	FUserCategoryRules& Rules = Settings->UserToolCategories.FindChecked(CategoryName); 

	// Try to find an entry containing the owning package.
	int FoundIndex = INDEX_NONE;
	for (int i = 0; i < Rules.Packages.Num(); ++i)
	{
		if (Rules.Packages[i].ToolsPackageAsset == Package)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// We don't have a record of this tool in the user categories settings.
		return;
	}

	FUserPackageRules PackageRules = Rules.Packages[FoundIndex];

	// Get the relative tool path and add it to the package rules.
	FString RelPath;
	ResolveHoudiniAssetRelativePath(Object, RelPath);

	// Remove all instances of this tool's path.
	PackageRules.Include.Remove(RelPath);

	if (PackageRules.Include.Num() == 0 && PackageRules.Exclude.Num() == 0)
	{
		// Remove this empty ruleset from the settings.
		Rules.Packages.RemoveAt(FoundIndex);
	}
	else
	{
		// We have a non-empty ruleset. write it back into user category settings.
		Rules.Packages[FoundIndex] = PackageRules;
	}

	FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(Settings, "UserToolCategories");
	
	Settings->SaveConfig();
}

bool
FHoudiniToolsEditor::UserCategoryContainsTool(
	const FString& CategoryName,
	const UObject* AssetObject,
	const UHoudiniToolsPackageAsset* PackageAsset)
{
	if (!IsValid(AssetObject))
	{
		return false;
	}
	
	if (!IsValid(PackageAsset))
	{
		return false;
	}

	UHoudiniEngineEditorSettings* Settings = GetMutableDefault<UHoudiniEngineEditorSettings>();
	if (!Settings)
	{
		return false;
	}

	if (!Settings->UserToolCategories.Contains(CategoryName))
	{
		return false;
	}

	FUserCategoryRules& Rules = Settings->UserToolCategories.FindChecked(CategoryName); 

	// Try to find an entry containing the owning package.
	int FoundIndex = INDEX_NONE;
	for (int i = 0; i < Rules.Packages.Num(); ++i)
	{
		if (Rules.Packages[i].ToolsPackageAsset == PackageAsset)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// We don't have a record of this tool in the user categories settings.
		return false;
	}

	const FUserPackageRules PackageRules = Rules.Packages[FoundIndex];

	// Get the relative tool path and add it to the package rules.
	FString RelPath;
	ResolveHoudiniAssetRelativePath(AssetObject, PackageAsset, RelPath);

	if (PackageRules.Include.Contains(RelPath))
		return true;

	return false;
}

void
FHoudiniToolsEditor::GetUserCategoriesList(TArray<FString>& OutCategories)
{
	OutCategories.Empty();
	const UHoudiniEngineEditorSettings* Settings = GetDefault<UHoudiniEngineEditorSettings>();
	if (!Settings)
	{
		return;
	}
	
	Settings->UserToolCategories.GetKeys(OutCategories);
}


void FHoudiniToolsEditor::CopySettingsToPreset(const UHoudiniAssetComponent* HAC,
	const bool bApplyAssetOptions,
	const bool bApplyBakeOptions,
	const bool bApplyMeshGenSettings,
	const bool bApplyProxyMeshGenSettings,
	UHoudiniPreset* Preset)
{
	// Populate Bake options
	Preset->bApplyBakeOptions = bApplyBakeOptions;
	Preset->HoudiniEngineBakeOption = HAC->HoudiniEngineBakeOption;
	Preset->bRemoveOutputAfterBake = HAC->bRemoveOutputAfterBake;
	Preset->bRecenterBakedActors = HAC->bRecenterBakedActors;
	Preset->bAutoBake = HAC->IsBakeAfterNextCookEnabled();
	Preset->bReplacePreviousBake = HAC->bReplacePreviousBake;

	// Populate Asset Settings
	Preset->bApplyAssetOptions = bApplyAssetOptions;
	Preset->bCookOnParameterChange = HAC->bCookOnParameterChange;
	Preset->bCookOnTransformChange = HAC->bCookOnTransformChange;
	Preset->bCookOnAssetInputCook = HAC->bCookOnAssetInputCook;
	Preset->bDoNotGenerateOutputs = HAC->bOutputless;
	Preset->bUseOutputNodes = HAC->bUseOutputNodes;
	Preset->bOutputTemplateGeos = HAC->bOutputTemplateGeos;

	Preset->bUploadTransformsToHoudiniEngine = HAC->bUploadTransformsToHoudiniEngine;
	Preset->bLandscapeUseTempLayers = HAC->bLandscapeUseTempLayers;

	// Populate Mesh Gen Settings
	Preset->bApplyStaticMeshGenSettings = bApplyMeshGenSettings;
	Preset->StaticMeshGenerationProperties = HAC->StaticMeshGenerationProperties;
	Preset->StaticMeshBuildSettings = HAC->StaticMeshBuildSettings;

	// Populate Proxy Mesh Gen Settings
	Preset->bApplyProxyMeshGenSettings = bApplyProxyMeshGenSettings;
	Preset->bOverrideGlobalProxyStaticMeshSettings = HAC->bOverrideGlobalProxyStaticMeshSettings;
	Preset->bEnableProxyStaticMeshOverride = HAC->bEnableProxyStaticMeshOverride;
	Preset->bEnableProxyStaticMeshRefinementByTimerOverride = HAC->bEnableProxyStaticMeshRefinementByTimerOverride;
	Preset->ProxyMeshAutoRefineTimeoutSecondsOverride = HAC->ProxyMeshAutoRefineTimeoutSecondsOverride;
	Preset->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = HAC->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
	Preset->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = HAC->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;
}


void
FHoudiniToolsEditor::FindPresetsForHoudiniAsset(const UHoudiniAsset* HoudiniAsset, TArray<UHoudiniPreset*>& OutPresets)
{
	if (!IsValid(HoudiniAsset))
	{
		return;
	}
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(UHoudiniPreset::StaticClass()), Assets, false);
#else
	AssetRegistryModule.Get().GetAssetsByClass( UHoudiniPreset::StaticClass()->GetFName(), Assets, false );
#endif
	
	// Currently we're scanning the whole project for presets. If this becomes a performance problem, we can limit preset
	// scanning to the HoudiniTools search path.

	for(const FAssetData& AssetData : Assets)
	{
		UHoudiniPreset* Preset = Cast<UHoudiniPreset>(AssetData.GetAsset());
		if (!IsValid(Preset))
		{
			continue;
		}
		
		if (Preset->bApplyOnlyToSource == false)
		{
			// This preset can be applied to any HDA.
			OutPresets.Add(Preset);
			continue;
		}
		if (Preset->bApplyOnlyToSource == true && Preset->SourceHoudiniAsset == HoudiniAsset)
		{
			// This preset can be applied to this HDA.
			OutPresets.Add(Preset);
			continue;
		}
	}
}


bool
FHoudiniToolsEditor::CanApplyPresetToHoudiniAssetcomponent(const UHoudiniPreset* Preset,
	UHoudiniAssetComponent* HAC)
{
	if (!IsValid(Preset))
	{
		return false;
	}
	if (!IsValid(HAC))
	{
		return false;
	}

	if (!Preset->bApplyOnlyToSource)
	{
		// We can apply this preset to any HoudiniAsset
		return true;
	}

	// We can only apply this preset to the SourceHoudiniAsset
	if (!IsValid(Preset->SourceHoudiniAsset))
	{
		return false;
	}
	return Preset->SourceHoudiniAsset == HAC->GetHoudiniAsset();
}


void
FHoudiniToolsEditor::ApplyPresetToHoudiniAssetComponent(
	const UHoudiniPreset* Preset,
	UHoudiniAssetComponent* HAC,
	bool bReselectSelectedActors)
{
	if (!IsValid(HAC) || !IsValid(Preset))
	{
		return;
	}

	if (!CanApplyPresetToHoudiniAssetcomponent(Preset, HAC))
	{
		return;
	}

		// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniPresets_ApplyToAssetComponent", "Apply Preset to Houdini Asset Component"),
		HAC->GetOuter());

	HAC->Modify();

	if (Preset->bRevertHDAParameters)
	{
		// Reset parameters to default values?
		for (int32 n = 0; n < HAC->GetNumParameters(); ++n)
		{
			UHoudiniParameter* Parm = HAC->GetParameterAt(n);

			if (IsValid(Parm) && !Parm->IsDefault())
			{
				Parm->RevertToDefault();
			}
		}
	}

	// Populate Bake options
	if (Preset->bApplyBakeOptions)
	{
		HAC->HoudiniEngineBakeOption = Preset->HoudiniEngineBakeOption;
		HAC->bRemoveOutputAfterBake = Preset->bRemoveOutputAfterBake;
		HAC->bRecenterBakedActors = Preset->bRecenterBakedActors;
		HAC->SetBakeAfterNextCook( Preset->bAutoBake ? EHoudiniBakeAfterNextCook::Always : EHoudiniBakeAfterNextCook::Disabled);
		HAC->bReplacePreviousBake = Preset->bReplacePreviousBake;
	}

	// Populate Asset Settings
	if (Preset->bApplyAssetOptions)
	{
		HAC->bCookOnParameterChange = Preset->bCookOnParameterChange;
		HAC->bCookOnTransformChange = Preset->bCookOnTransformChange;
		HAC->bCookOnAssetInputCook = Preset->bCookOnAssetInputCook;
		HAC->bOutputless = Preset->bDoNotGenerateOutputs;
		HAC->bUseOutputNodes = Preset->bUseOutputNodes;
		HAC->bOutputTemplateGeos = Preset->bOutputTemplateGeos;
		HAC->bUploadTransformsToHoudiniEngine = Preset->bUploadTransformsToHoudiniEngine;
		HAC->bLandscapeUseTempLayers = Preset->bLandscapeUseTempLayers;
	}

	FHoudiniParameterTranslator::UpdateParameters(HAC);

	// Iterate over all the parameters and settings in the preset and apply it to the Houdini Asset Component.

	// Apply all Multiparam parameters. Since multiparms may contain multiparms we need to perform a loop
	// setting as many multiparms as we can, then updating parameters, then setting the remaining multiparams
	// again, and repeat. Until we end up with no multiparams remaining or nothing changed on the last
	// parameter update.

	TSet<FString> UnprocessedMultiParms;
	
	Preset->MultiParmParameters.GetKeys(UnprocessedMultiParms);

	while(!UnprocessedMultiParms.IsEmpty())
	{
		// Create a temp array of all param names we haven't processed yet. Do this so we can modify UnprocessedMultiParms
		// in the loop below.
		TArray<FString> CurrentParms;
		for (const FString& Element : UnprocessedMultiParms)
			CurrentParms.Add(Element);

		bool bProcessedAtLeastOne = false;

		// now loop over all parms we haven't process and try to set them. If we can set them, remove them from the
		// unprocessed set.
		for (FString& ParmName : CurrentParms)
		{
			const FHoudiniPresetMultiParmValues& ParmValues = Preset->MultiParmParameters[ParmName];

			UHoudiniParameter* Parm = HAC->FindParameterByName(ParmName);
			if (!IsValid(Parm))
				continue;

			FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterMultiParm>(Parm));
			UnprocessedMultiParms.Remove(ParmName);
			bProcessedAtLeastOne = true;
		}
		if (!bProcessedAtLeastOne)
			break;

		FHoudiniParameterTranslator::UploadChangedParameters(HAC);
		FHoudiniParameterTranslator::UpdateParameters(HAC);
	}


	if (Preset->MultiParmParameters.Num() > 0)
	{
		FHoudiniParameterTranslator::UploadChangedParameters(HAC);
		FHoudiniParameterTranslator::UpdateParameters(HAC);
	}

	// Apply all the Int parameters
	for( const auto& Entry : Preset->IntParameters )
	{
		const FString& ParmName = Entry.Key;
		const FHoudiniPresetIntValues& ParmValues = Entry.Value;
		
		UHoudiniParameter* Parm = HAC->FindParameterByName( ParmName );
		if (!IsValid(Parm))
		{
			continue;
		}
		
		const EHoudiniParameterType ParmType = Parm->GetParameterType();
		switch(ParmType)
		{
			case EHoudiniParameterType::Int:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterInt>(Parm));
				break;
			case EHoudiniParameterType::IntChoice:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterChoice>(Parm));
				break;
			case EHoudiniParameterType::Toggle:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterToggle>(Parm));
				break;
			default: ;
		}
	}

	// Apply all the Float parameters
	for( const auto& Entry : Preset->FloatParameters )
	{
		const FString& ParmName = Entry.Key;
		const FHoudiniPresetFloatValues& ParmValues = Entry.Value;

		UHoudiniParameter* Parm = HAC->FindParameterByName( ParmName );
		if (!IsValid(Parm))
		{
			continue;
		}
		
		const EHoudiniParameterType ParmType = Parm->GetParameterType();
		switch(ParmType)
		{
			case EHoudiniParameterType::Color:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterColor>(Parm));
				break;
			case EHoudiniParameterType::Float:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterFloat>(Parm));
				break;
			default: ;
		}
	}

	// Apply all the String parameters

	for( const auto& Entry : Preset->StringParameters )
	{
		const FString& ParmName = Entry.Key;
		const FHoudiniPresetStringValues& ParmValues = Entry.Value;

		UHoudiniParameter* Parm = HAC->FindParameterByName( ParmName );
		if (!IsValid(Parm))
		{
			continue;
		}
		
		const EHoudiniParameterType ParmType = Parm->GetParameterType();
		switch(ParmType)
		{
			case EHoudiniParameterType::File:
			case EHoudiniParameterType::FileDir:
			case EHoudiniParameterType::FileGeo:
			case EHoudiniParameterType::FileImage:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterFile>(Parm));
				break;
			case EHoudiniParameterType::String:
			case EHoudiniParameterType::StringAssetRef:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterString>(Parm));
				break;
			case EHoudiniParameterType::StringChoice:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterChoice>(Parm));
				break;
			default: ;
		}
	}

	// Apply all the Ramp Float parameters
	for( const auto& Entry : Preset->RampFloatParameters )
	{
		const FString& ParmName = Entry.Key;
		const FHoudiniPresetRampFloatValues& ParmValues = Entry.Value;

		UHoudiniParameter* Parm = HAC->FindParameterByName( ParmName );
		if (!IsValid(Parm))
		{
			continue;
		}
		
		const EHoudiniParameterType ParmType = Parm->GetParameterType();
		switch(ParmType)
		{
			case EHoudiniParameterType::FloatRamp:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterRampFloat>(Parm));
				break;
			default: ;
		}
	}

	// Apply all the Ramp Color parameters
	for( const auto& Entry : Preset->RampColorParameters )
	{
		const FString& ParmName = Entry.Key;
		const FHoudiniPresetRampColorValues& ParmValues = Entry.Value;

		UHoudiniParameter* Parm = HAC->FindParameterByName( ParmName );
		if (!IsValid(Parm))
		{
			continue;
		}
		
		const EHoudiniParameterType ParmType = Parm->GetParameterType();
		switch(ParmType)
		{
			case EHoudiniParameterType::ColorRamp:
				FHoudiniPresetHelpers::ApplyPresetParameterValues(ParmValues, Cast<UHoudiniParameterRampColor>(Parm));
				break;
			default: ;
		}
	}

	// Apply inputs
	for(const FHoudiniPresetInputValue& PresetInput : Preset->InputParameters )
	{
		if (PresetInput.InputType == EHoudiniInputType::Invalid)
		{
			continue;
		}
		
		if (PresetInput.bIsParameterInput)
		{
			// Parameter based input
			UHoudiniParameterOperatorPath* Param = Cast<UHoudiniParameterOperatorPath>( HAC->FindParameterByName(PresetInput.ParameterName) );
			if (IsValid(Param))
			{
				UHoudiniInput* Input = Param->HoudiniInput.Get();
				FHoudiniPresetHelpers::ApplyPresetParameterValues(PresetInput, Input);
			}
		}
		else
		{
			// Absolute input
			UHoudiniInput* Input = HAC->GetInputAt( PresetInput.InputIndex );
			FHoudiniPresetHelpers::ApplyPresetParameterValues(PresetInput, Input);
		}
	}

	if (Preset->bApplyStaticMeshGenSettings)
	{
		HAC->StaticMeshGenerationProperties = Preset->StaticMeshGenerationProperties;
		HAC->StaticMeshBuildSettings = Preset->StaticMeshBuildSettings;
	}

	if (Preset->bApplyProxyMeshGenSettings)
	{
		// Populate Proxy Mesh Gen Settings
		HAC->bOverrideGlobalProxyStaticMeshSettings = Preset->bOverrideGlobalProxyStaticMeshSettings;
		HAC->bEnableProxyStaticMeshOverride = Preset->bEnableProxyStaticMeshOverride;
		HAC->bEnableProxyStaticMeshRefinementByTimerOverride = Preset->bEnableProxyStaticMeshRefinementByTimerOverride;
		HAC->ProxyMeshAutoRefineTimeoutSecondsOverride = Preset->ProxyMeshAutoRefineTimeoutSecondsOverride;
		HAC->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = Preset->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
		HAC->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = Preset->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;
	}

	if (bReselectSelectedActors)
	{
		FHoudiniEngineEditorUtils::ReselectSelectedActors();
	}

	for(auto & Callback : Preset->PostInstantiationCallbacks)
	{
		Callback(Preset, HAC);
	}
}

void FHoudiniToolsEditor::ApplyObjectsAsHoudiniAssetInputs(
	const TMap<UObject*, int32>& InputObjects,
	UHoudiniAssetComponent* HAC
	)
{
	if (InputObjects.Num() <= 0)
		return;

#if WITH_EDITOR

	struct FInputTypeCount
	{
		FInputTypeCount(): NumWorldInputs(0), NumGeoInputs(0), NumCurveInputs(0) {}

		int32 NumWorldInputs;
		int32 NumGeoInputs;
		int32 NumCurveInputs;
	};
	
	// Ignore inputs that have been preset to curve
	TArray<UHoudiniInput*> InputArray;
	TArray<int32> InputIsCleared;
	TMap<int32, FInputTypeCount> TypeCountMap;
	
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++)
	{
		UHoudiniInput* CurrentInput = HAC->GetInputAt(InputIndex);
		
		if (!IsValid(CurrentInput))
			continue;
		
		InputArray.Add(CurrentInput);
		InputIsCleared.Add(false);
	}

	// Try to apply the supplied Object to the Input
	for (TMap< UObject*, int32 >::TConstIterator IterToolPreset(InputObjects); IterToolPreset; ++IterToolPreset)
	{
		UObject * Object = IterToolPreset.Key();
		if (!IsValid(Object))
			continue;

		const int32 InputNumber = IterToolPreset.Value();
		if (!InputArray.IsValidIndex(InputNumber))
			continue;

		FInputTypeCount& Count = TypeCountMap.FindOrAdd(InputNumber);


		UBlueprint* BlueprintObj = Cast<UBlueprint>(Object);

		// If the object is a landscape, add a new landscape input
		if (Object->IsA<ALandscapeProxy>())
		{
			// selecting a landscape 
			int32 InsertNum = Count.NumWorldInputs;
			InputArray[InputNumber]->SetInputObjectAt(EHoudiniInputType::World, InsertNum, Object);
			Count.NumWorldInputs++;
		}
		// If the object is a static mesh, add a new geometry input (TODO: or BP ? )
		else if (Object->IsA<UStaticMesh>())
		{
			// selecting a Static Mesh
			int32 InsertNum = Count.NumGeoInputs;
			InputArray[InputNumber]->SetInputObjectAt(EHoudiniInputType::Geometry, InsertNum, Object);
			Count.NumGeoInputs++;
		}
		else if (IsValid(BlueprintObj) && BlueprintObj->BlueprintType == EBlueprintType::BPTYPE_Normal)
		{
			// selecting a normal Blueprint asset as Geometry input type
			int32 InsertNum = Count.NumGeoInputs;
			InputArray[InputNumber]->SetInputObjectAt(EHoudiniInputType::Geometry, InsertNum, Object);
			Count.NumGeoInputs++;
		}
		else if (Object->IsA<AHoudiniAssetActor>())
		{
			// selecting a Houdini Asset 
			int32 InsertNum = Count.NumWorldInputs;
			InputArray[InputNumber]->SetInputObjectAt(EHoudiniInputType::World, InsertNum, Object);
			Count.NumWorldInputs++;
		}
		// If the object is an actor, add a new world input
		else if (Object->IsA<AActor>())
		{
			// selecting a generic actor 
			int32 InsertNum = Count.NumWorldInputs;
			InputArray[InputNumber]->SetInputObjectAt(EHoudiniInputType::World, InsertNum, Object);
			Count.NumWorldInputs++;
		}
	}

	bool bBPStructureModified = false;
	
	// We try to guess the input type based on the count of the received input objects.
	for (const auto& Entry : TypeCountMap)
	{
		const int32 InputIndex = Entry.Key;
		const FInputTypeCount& Count = Entry.Value;

		if (!InputArray.IsValidIndex(InputIndex) || !IsValid(InputArray[InputIndex]))
		{
			continue;
		}
		UHoudiniInput* Input = InputArray[InputIndex];
		if (Count.NumWorldInputs > 0 && Count.NumWorldInputs >= Count.NumGeoInputs && Count.NumWorldInputs >= Count.NumCurveInputs)
		{
			Input->SetInputType(EHoudiniInputType::World, bBPStructureModified);
		}
		else if (Count.NumGeoInputs >= 0 && Count.NumGeoInputs >= Count.NumCurveInputs)
		{
			Input->SetInputType(EHoudiniInputType::Geometry, bBPStructureModified);
		}
		else if (Count.NumGeoInputs >= 0 && Count.NumGeoInputs >= Count.NumCurveInputs)
		{
			Input->SetInputType(EHoudiniInputType::Geometry, bBPStructureModified);
		}
		else
		{
			// We don't know what input type this is. Skip it.
			continue;
		}
	}

	if (bBPStructureModified)
	{
		HAC->MarkAsBlueprintStructureModified();
	}
#endif
	
}

void FHoudiniToolsEditor::ApplyPresetToSelectedHoudiniAssetActors(const UHoudiniPreset* Preset, bool bReselectSelectedActors)
{
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	SelectedActors.SetNum(GEditor->GetSelectedActorCount());
	Selection->GetSelectedObjects(SelectedActors);

	for (AActor* SelectedActor : SelectedActors)
	{
		const AHoudiniAssetActor* HouActor = Cast<AHoudiniAssetActor>(SelectedActor);
		if (IsValid(HouActor))
		{
			UHoudiniAssetComponent* HAC = HouActor->GetHoudiniAssetComponent();
			if (IsValid(HAC))
			{
				ApplyPresetToHoudiniAssetComponent(Preset, HAC, false);
			}
		}
	}

	if (bReselectSelectedActors)
	{
		FHoudiniEngineEditorUtils::ReselectSelectedActors();
	}
}


void
FHoudiniToolsEditor::LaunchHoudiniToolPropertyEditor(const TSharedPtr<FHoudiniTool> ToolData)
{
	if (!ToolData.IsValid())
		return;
	
	UHoudiniAsset* HoudiniAsset = ToolData->HoudiniAsset.LoadSynchronous();
	UHoudiniPreset* HoudiniPreset = ToolData->HoudiniPreset.LoadSynchronous();
	UObject* AssetObject = nullptr;
	FString PathName;

	if (ToolData->PackageToolType == EHoudiniPackageToolType::HoudiniAsset)
	{
		if (!IsValid(HoudiniAsset))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not launch HoudiniTool Property Editor. Invalid HoudiniAsset."));
			return;
		}

		AssetObject = HoudiniAsset;
		PathName = HoudiniAsset->GetPathName();
	}

	if (ToolData->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		if (!IsValid(HoudiniPreset))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not launch HoudiniTool Property Editor. Invalid HoudiniPreset."));
			return;
		}
		AssetObject = HoudiniPreset;
		PathName = HoudiniPreset->GetPathName();
	}

	const FName ViewIdentifier = FName(TEXT("HoudiniToolPropertyEditor:") + PathName);

	// See if we can find an existing property editor for this asset

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedPtr<class IDetailsView> DetailsView = PropertyEditorModule.FindDetailView(ViewIdentifier);
	if (DetailsView.IsValid())
	{
		TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(DetailsView->AsShared());
		if ( ContainingWindow.IsValid() )
		{
			ContainingWindow->BringToFront();
		}
		// Focus the existing details view
		return;
	}
	
	// Create a new Tool property object for the property dialog
	FString ToolName = ToolData->Name.ToString();
	if (IsValid(HoudiniAsset))
	{
		ToolName += TEXT(" (") + HoudiniAsset->AssetFileName + TEXT(")");
	}
	
	UHoudiniToolEditorProperties* ToolProperties = NewObject<UHoudiniToolEditorProperties>( GetTransientPackage(), FName( *ToolName ) );
	// ToolProperties->AddToRoot();

	// Set the default values for this asset
	ToolProperties->Name = ToolData->Name.ToString();
	ToolProperties->Type = ToolData->Type;
	ToolProperties->ToolTip = ToolData->ToolTipText.ToString();
	ToolProperties->HelpURL = ToolData->HelpURL;
	ToolProperties->SelectionType = ToolData->SelectionType;
	// Always leave this field blank. The user can use this to import a new icon from an arbitrary location. 
	ToolProperties->IconPath.FilePath = FString();
	ToolProperties->ToolType = ToolData->PackageToolType;
	ToolProperties->HoudiniAsset = HoudiniAsset;
	ToolProperties->HoudiniPreset = HoudiniPreset;

	TArray<UObject *> ActiveHoudiniTools;
	ActiveHoudiniTools.Add( ToolProperties );


	TSharedPtr<FHoudiniTool> EditingTool = ToolData;

	// Create a new property editor window
	TSharedRef< SWindow > Window = CreateFloatingDetailsView(
		ActiveHoudiniTools,
		ViewIdentifier,
		FVector2D(450,650),
		[EditingTool](TArray<UObject*> InObjects)
		{
			switch (EditingTool->PackageToolType)
			{
				case EHoudiniPackageToolType::HoudiniAsset:
					HandleHoudiniAssetPropertyEditorSaveClicked(EditingTool, InObjects);
					break;
				case EHoudiniPackageToolType::Preset:
					HandleHoudiniPresetPropertyEditorSaveClicked(EditingTool, InObjects);
					break;
				default:
					HOUDINI_LOG_ERROR(TEXT("Could not save due to unrecognized PackageToolType."));
			}
		}
	);
}


TSharedRef<SWindow>
FHoudiniToolsEditor::CreateFloatingDetailsView(
	TArray<UObject*>& InObjects,
	FName InViewIdentifier,
	const FVector2D InClientSize, const TFunction<void(TArray<UObject*>)> OnSaveClickedFn)
{
	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
		.Title(NSLOCTEXT("PropertyEditor", "WindowTitle", "Houdini Tools Property Editor"))
		.ClientSize(InClientSize);

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if ( FModuleManager::Get().IsModuleLoaded("MainFrame") )
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if ( ParentWindow.IsValid() )
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( NewSlateWindow );
	}

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = false;
	Args.bAllowMultipleTopLevelObjects = true;
	Args.ViewIdentifier = InViewIdentifier;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bShowPropertyMatrixButton = false;
	Args.bShowOptions = false;
	Args.bShowModifiedPropertiesOption = false;
	Args.bShowObjectLabel = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const TSharedRef<IDetailsView> DetailView = PropertyEditorModule.CreateDetailView( Args );
	DetailView->SetObjects( InObjects );
	TWeakPtr<SWindow> WindowWeakPtr = NewSlateWindow;

	NewSlateWindow->SetContent(
		SNew( SBorder )
		.BorderImage(_GetBrush(TEXT("PropertyWindow.WindowBorder")))
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SVerticalBox)

			// Detail View
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailView
			]

			// Button Row
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			.FillHeight(1.f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))

				// Save Button
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&_GetEditorStyle().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.Content()
					[
						SNew(STextBlock)
						.TextStyle( &_GetEditorStyle().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText") )
						.Text( LOCTEXT("HoudiniTools_Details_Save","Save") )
					]
					.OnClicked_Lambda([WindowWeakPtr, OnSaveClickedFn, InObjects]() -> FReply
					{
						OnSaveClickedFn(InObjects);
						if (WindowWeakPtr.IsValid())
						{
							WindowWeakPtr.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				// Cancel Button
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&_GetEditorStyle().GetWidgetStyle<FButtonStyle>("Button"))
					.Content()
					[
						SNew(STextBlock)
						.TextStyle( &_GetEditorStyle().GetWidgetStyle<FTextBlockStyle>("ButtonText") )
						.Text( LOCTEXT("HoudiniTools_Details_Cancel","Cancel") )
					]
					.OnClicked_Lambda([WindowWeakPtr]() -> FReply
					{
						if (WindowWeakPtr.IsValid())
						{
							WindowWeakPtr.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			] // Button Row
		] // SBorder
	);

	return NewSlateWindow;
}


void
FHoudiniToolsEditor::HandleHoudiniAssetPropertyEditorSaveClicked(TSharedPtr<FHoudiniTool> InToolData, TArray<UObject *>& InObjects)
{
	// Sanity check, we can only edit one tool at a time!
	if ( InObjects.Num() != 1 )
		return;

	if (!InToolData.IsValid())
		return;

	checkf(InToolData->PackageToolType == EHoudiniPackageToolType::HoudiniAsset, TEXT("This function should only be called for HoudiniAsset tools."));

	UHoudiniAsset* HoudiniAsset = InToolData->HoudiniAsset.LoadSynchronous();
	if (!HoudiniAsset)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not locate active tool. Unable to save changes."));
		return;
	}

	// Reimport assets from their new sources.
	TArray<UHoudiniAsset*> ReimportAssets;

	TArray< FHoudiniTool > EditedToolArray;
	for ( int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++ )
	{
		UHoudiniToolEditorProperties* ToolProperties = Cast< UHoudiniToolEditorProperties >( InObjects[ ObjIdx ] );
		if ( !ToolProperties )
			continue;

		// FString IconPath = FPaths::ConvertRelativePathToFull( ToolProperties->IconPath.FilePath );
		// const FSlateBrush* CustomIconBrush = nullptr;
		// if ( FPaths::FileExists( IconPath ) )
		// {
		//
		//     // If we have a valid icon path, load the file. 
		//     FName BrushName = *IconPath;
		//     CustomIconBrush = new FSlateDynamicImageBrush( BrushName, FVector2D( 40.f, 40.f ) );
		// }
		
		//  - Store all the edited properties on the HoudiniToolData.
		//  - Populate the relevant FHoudiniTool descriptor from the HoudiniAsset.
		//  - Save out the JSON description, if required.
		
		bool bModified = false;

		// Helper macro for Property assignments and modify flag management
		#define ASSIGNFN(Src, Dst) \
		{\
			if (Src != Dst)\
			{\
				bModified = true;\
				Dst = Src;\
			}\
		}

		UHoudiniToolData* ToolData = FHoudiniToolsEditor::GetOrCreateHoudiniToolData(HoudiniAsset);
		ToolData->Modify();
		ASSIGNFN(ToolProperties->Name, ToolData->Name);
		ASSIGNFN(ToolProperties->Type, ToolData->Type);
		ASSIGNFN(ToolProperties->SelectionType, ToolData->SelectionType);
		ASSIGNFN(ToolProperties->ToolTip, ToolData->ToolTip);
		ASSIGNFN(ToolProperties->HelpURL, ToolData->HelpURL);

		if (FPaths::FileExists(ToolProperties->AssetPath.FilePath))
		{
			// If the AssetPath has changed, reimport the HDA with the new source
			if (ToolProperties->AssetPath.FilePath != ToolData->SourceAssetPath.FilePath)
			{
				TArray<FString> Filenames;
				Filenames.Add(ToolProperties->AssetPath.FilePath);
				FReimportManager::Instance()->UpdateReimportPaths(HoudiniAsset, Filenames);
				
				ReimportAssets.Add(HoudiniAsset);
			}
			ASSIGNFN(ToolProperties->AssetPath.FilePath, ToolData->SourceAssetPath.FilePath);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("The specified AssetPath does not exist. Source Asset Path will remain unchanged"));
		}

		

		bool bModifiedIcon = false;

		if (ToolProperties->IconPath.FilePath.Len() > 0)
		{
			bModified = true;
			bModifiedIcon = true;
			ToolData->LoadIconFromPath(ToolProperties->IconPath.FilePath);
		}
		else if (ToolProperties->bClearCachedIcon)
		{
			bModified = true;
			bModifiedIcon = true;
			ToolData->ClearCachedIcon();
		}
		else
		{
			// If we're not changed the icon, at least ensure our thumbnail icon is up to date
			FHoudiniToolsRuntimeUtils::UpdateAssetThumbnailFromImageData(HoudiniAsset, ToolData->IconImageData);
		}

		if (bModifiedIcon)
		{
			// Ensure the content browser reflects icon changes.
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().OnAssetUpdated().Broadcast(FAssetData(HoudiniAsset));
		}
		
		ToolData->DefaultTool = false;
		
		if (bModified)
		{
			ToolData->MarkPackageDirty();
			// We modified the HoudiniAsset. Request a panel refresh.
			HoudiniAsset->PostEditChange();
		}

		// Remove the tool from Root
		ToolProperties->RemoveFromRoot();
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	

	for (UHoudiniAsset* Asset : ReimportAssets)
	{
		FReimportManager::Instance()->Reimport(Asset, false /* Ask for new file */, true /* Show notification */);
	}
}


void
FHoudiniToolsEditor::HandleHoudiniPresetPropertyEditorSaveClicked(TSharedPtr<FHoudiniTool> InToolData, TArray<UObject *>& InObjects)
{
	// Sanity check, we can only edit one tool at a time!
	if ( InObjects.Num() != 1 )
		return;

	if (!InToolData.IsValid())
		return;

	checkf(InToolData->PackageToolType == EHoudiniPackageToolType::Preset, TEXT("This function should only be called for HoudiniPreset tools."));

	// UHoudiniAsset* HoudiniAsset = InToolData->HoudiniAsset.LoadSynchronous();
	// if (!HoudiniAsset)
	// {
	// 	HOUDINI_LOG_ERROR(TEXT("Could not locate active tool. Unable to save changes."));
	// 	return;
	// }

	UHoudiniPreset* HoudiniPreset = InToolData->HoudiniPreset.LoadSynchronous();
	if (!HoudiniPreset)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not locate active tool. Unable to save changes."));
		return;
	}

	// Reimport assets from their new sources.
	TArray<UHoudiniAsset*> ReimportAssets;

	TArray< FHoudiniTool > EditedToolArray;
	for ( int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++ )
	{
		UHoudiniToolEditorProperties* ToolProperties = Cast< UHoudiniToolEditorProperties >( InObjects[ ObjIdx ] );
		if ( !ToolProperties )
			continue;
		
		bool bModified = false;

		// Helper macro for Property assignments and modify flag management
		#define ASSIGNFN(Src, Dst) \
		{\
			if (Src != Dst)\
			{\
				bModified = true;\
				Dst = Src;\
			}\
		}

		HoudiniPreset->Modify();
		
		ASSIGNFN(ToolProperties->Name, HoudiniPreset->Name);
		ASSIGNFN(ToolProperties->ToolTip, HoudiniPreset->Description);
		
		bool bModifiedIcon = false;
		
		if (ToolProperties->IconPath.FilePath.Len() > 0)
		{
			bModified = true;
			bModifiedIcon = true;
			FHoudiniToolsRuntimeUtils::LoadFHImageFromFile( ToolProperties->IconPath.FilePath, HoudiniPreset->IconImageData );
			FHoudiniToolsRuntimeUtils::UpdateAssetThumbnailFromImageData(HoudiniPreset, HoudiniPreset->IconImageData);
		}
		else if (ToolProperties->bClearCachedIcon)
		{
			bModified = true;
			bModifiedIcon = true;
			HoudiniPreset->IconImageData = FHImageData();
			FHoudiniToolsRuntimeUtils::UpdateAssetThumbnailFromImageData(HoudiniPreset, FHImageData());
		}
		else
		{
			// If we're not changed the icon, at least ensure our thumbnail icon is up to date
			FHoudiniToolsRuntimeUtils::UpdateAssetThumbnailFromImageData(HoudiniPreset, HoudiniPreset->IconImageData);
		}
		
		if (bModifiedIcon)
		{
			// Ensure the content browser reflects icon changes.
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().OnAssetUpdated().Broadcast(FAssetData(HoudiniPreset));
		}
		
		if (bModified)
		{
			HoudiniPreset->MarkPackageDirty();
			// We modified an existing tool. Call PostEditChange to trigger HoudiniPanel update.
			HoudiniPreset->PostEditChange();
		}
		
		// Remove the tool from Root
		ToolProperties->RemoveFromRoot();
	}
}


void
FHoudiniToolsEditor::Shutdown()
{
	Categories.Empty();
	for  (auto& Entry : CachedTextures)
	{
		UTexture2D* CachedTexture = Entry.Value;
		if(CachedTexture && CachedTexture->IsValidLowLevel())
		{
			CachedTexture->RemoveFromRoot();
		}
	}
	CachedTextures.Empty();
}



#undef LOCTEXT_NAMESPACE
