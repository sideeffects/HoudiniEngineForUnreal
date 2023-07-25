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

#include "Editor.h"
#include "EditorReimportHandler.h"
#include "GameProjectUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolsPackageAssetFactory.h"
#include "JsonObjectConverter.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HoudiniToolsRuntimeUtils.h"


#define LOCTEXT_NAMESPACE "HoudiniTools"

DEFINE_LOG_CATEGORY(LogHoudiniTools);

// Wrapper interface to manage code compatibility across multiple UE versions

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
// Unreal 5.0 implementations
#include "EditorAssetLibrary.h"
struct FToolsWrapper
{
    static bool DoesAssetExist(const FString& AssetPath)
    {
        return UEditorAssetLibrary::DoesAssetExist(AssetPath);
    };

    static FName ConvertAssetObjectPath(const FString& Name) { return FName(Name); }

    static void CheckValidEditorAssetSubsystem() {}

    static void ReimportAsync(UObject* Object, bool bAskForNewFileIfMissing, bool bShowNotification, const FString& PreferredReimportFile = TEXT(""))
    {
        FReimportManager::Instance()->Reimport(Object, bAskForNewFileIfMissing, bShowNotification, PreferredReimportFile);
    }

    static bool DoesDirectoryExist(const FString& DirectoryPath)
    {
        return UEditorAssetLibrary::DoesDirectoryExist(DirectoryPath);
    }

    static bool MakeDirectory(const FString& DirectoryPath)
    {
        return UEditorAssetLibrary::DoesDirectoryExist(DirectoryPath);
    }
};

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
// Unreal 5.1+ implementations
#include "Subsystems/EditorAssetSubsystem.h"
struct FToolsWrapper
{
    static bool DoesAssetExist(const FString& AssetPath)
    {
        UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	    return EditorAssetSubsystem->DoesAssetExist(AssetPath);
    }

    static FSoftObjectPath ConvertAssetObjectPath(const FString& Name) { return FSoftObjectPath(Name); }

    static void CheckValidEditorAssetSubsystem()
    {
        UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
        checkf(EditorAssetSubsystem != nullptr, TEXT("Invalid EditorAssetSubsystem"));
    }

    static void ReimportAsync(UObject* Object, bool bAskForNewFileIfMissing, bool bShowNotification, const FString& PreferredReimportFile=TEXT(""))
    {
        FReimportManager::Instance()->ReimportAsync(Object, bAskForNewFileIfMissing, bShowNotification, PreferredReimportFile);
    }

    static bool DoesDirectoryExist(const FString& DirectoryPath)
    {
        UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
        return EditorAssetSubsystem->DoesDirectoryExist(DirectoryPath);
    }

    static bool MakeDirectory(const FString& DirectoryPath)
    {
        UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
        return EditorAssetSubsystem->DoesDirectoryExist(DirectoryPath);
    }
};

#endif


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
    return FPaths::Combine(FPaths::GetPath(FPaths::GetProjectFilePath()), ToolsPackage->ExternalPackageDir.Path);
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
FHoudiniToolsEditor::ResolveHoudiniAssetRelativePath(const UHoudiniAsset* HoudiniAsset, FString& OutPath)
{
    if (!IsValid(HoudiniAsset))
        return false;

    const UHoudiniToolsPackageAsset* ToolsPackage = FindOwningToolsPackage(HoudiniAsset);
    if (!IsValid(ToolsPackage))
        return false;

    //NOTE: We're constructing a relative path here for the purposes of include/exclude pattern matching.
    //      to keep things as straightforward as possible, we always use the path name of the relevant
    //      asset for pattern matching. We will NOT be using the operator type or the tool label.
    
    const FString ToolsPackagePath = ToolsPackage->GetPackage()->GetPathName();

    // Use the HoudiniAsset's relative (to the Package descriptor) path  for category matching.
    OutPath = HoudiniAsset->GetPackage()->GetPathName();
    FPaths::MakePathRelativeTo(OutPath, *ToolsPackagePath);
    
    return true;
}


// void
// FHoudiniTools::GetAllHoudiniToolDirectories( TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray ) const
// {
//     HoudiniToolsDirectoryArray.Empty();
//
//     // Read the default tools from the $HFS/engine/tool folder
//     FDirectoryPath DefaultToolPath;
//     FString HFSPath = FPaths::GetPath(FHoudiniEngine::Get().GetLibHAPILocation());
//     FString DefaultPath = FPaths::Combine(HFSPath, TEXT("engine"));
//     DefaultPath = FPaths::Combine(DefaultPath, TEXT("tools"));
//
//     FHoudiniToolDirectory ToolDir;
//     ToolDir.Name = TEXT("Default");
//     ToolDir.Path.Path = DefaultPath;
//     ToolDir.ContentDirID = TEXT("Default");
//
//     HoudiniToolsDirectoryArray.Add( ToolDir );
//
//     // Append all the custom tools directory from the runtime settings
//     // UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetMutableDefault< UHoudiniRuntimeSettings >();
//     // if ( !HoudiniRuntimeSettings )
//     //     return;
//
// 	// TODO: We need to replace the CustomToolsLocations
//     // We have to make sure all Custom tool dir have a ContentDirID
//     bool NeedToSave = false;
//     for (int32 n = 0; n < CustomHoudiniToolsLocation.Num(); n++ )
//     {
//         if (!CustomHoudiniToolsLocation[n].ContentDirID.IsEmpty())
//             continue;
//
//         // Generate a new Directory ID for that directory
//         CustomHoudiniToolsLocation[n].ContentDirID = ObjectTools::SanitizeObjectName(
//             CustomHoudiniToolsLocation[n].Name + TEXT(" ") + FGuid::NewGuid().ToString() );
//
//         NeedToSave = true;
//     }
//
//     // if ( NeedToSave )
//     //     HoudiniRuntimeSettings->SaveConfig();
//
//     // Add all the custom tool paths
//     HoudiniToolsDirectoryArray.Append( CustomHoudiniToolsLocation );
// }

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
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
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

    

 //    // Read the default tools from the $HFS/engine/tool folder
 //    FDirectoryPath DefaultToolPath;
 //    FString HFSPath = FPaths::GetPath(FHoudiniEngine::Get().GetLibHAPILocation());
 //    FString DefaultPath = FPaths::Combine(HFSPath, TEXT("engine"));
 //    DefaultPath = FPaths::Combine(DefaultPath, TEXT("tools"));
 //
 //    FHoudiniToolDirectory ToolDir;
 //    ToolDir.Name = TEXT("Default");
 //    ToolDir.Path.Path = DefaultPath;
 //    ToolDir.ContentDirID = TEXT("Default");
 //
 //    HoudiniToolsDirectoryArray.Add( ToolDir );
 //
 //    // Append all the custom tools directory from the runtime settings
 //    // UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetMutableDefault< UHoudiniRuntimeSettings >();
 //    // if ( !HoudiniRuntimeSettings )
 //    //     return;
 //
	// // TODO: We need to replace the CustomToolsLocations
 //    // We have to make sure all Custom tool dir have a ContentDirID
 //    bool NeedToSave = false;
 //    for (int32 n = 0; n < CustomHoudiniToolsLocation.Num(); n++ )
 //    {
 //        if (!CustomHoudiniToolsLocation[n].ContentDirID.IsEmpty())
 //            continue;
 //
 //        // Generate a new Directory ID for that directory
 //        CustomHoudiniToolsLocation[n].ContentDirID = ObjectTools::SanitizeObjectName(
 //            CustomHoudiniToolsLocation[n].Name + TEXT(" ") + FGuid::NewGuid().ToString() );
 //
 //        NeedToSave = true;
 //    }
 //
 //    // if ( NeedToSave )
 //    //     HoudiniRuntimeSettings->SaveConfig();
 //
 //    // Add all the custom tool paths
 //    HoudiniToolsDirectoryArray.Append( CustomHoudiniToolsLocation );
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
    
    for (const FString PackagePath : PackagePaths)
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
FHoudiniToolsEditor::FindOwningToolsPackage(const UHoudiniAsset* HoudiniAsset)
{
    if (!IsValid(HoudiniAsset))
        return nullptr;

    UHoudiniToolsPackageAsset* Package = nullptr;
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

void FHoudiniToolsEditor::FindHoudiniToolsInPackage(const UHoudiniToolsPackageAsset* ToolsPackage, TArray<TSharedPtr<FHoudiniTool>>& OutHoudiniTools) const
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
        if (!AssetData.IsInstanceOf(UHoudiniAsset::StaticClass()))
        {
            continue;
        }
        const UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>( AssetData.GetAsset() );
        if (!IsValid(HoudiniAsset))
            continue;

        // Construct FHoudiniTool and populate from the current Houdini Asset and package.
        TSharedPtr<FHoudiniTool> HoudiniTool = MakeShareable( new FHoudiniTool() );

        PopulateHoudiniTool(HoudiniTool, HoudiniAsset, ToolsPackage, false);
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
    const TMap<FString, FCategoryRules>& InCategories,
    const TSharedPtr<FHoudiniTool>& HoudiniTool,
    const bool bIgnoreExclusionRules,
    TArray<FString>& OutCategories)
{
    if (!HoudiniTool.IsValid())
        return;
    
    const UHoudiniAsset* HoudiniAsset = HoudiniTool->HoudiniAsset.LoadSynchronous();
    if (!IsValid(HoudiniAsset))
        return;

    //NOTE: We're constructing a relative path here for the purposes of include/exclude pattern matching.
    //      to keep things as straightforward as possible, we always use the path name of the relevant
    //      asset for pattern matching. We will NOT use the operator type or the tool label.
    
    FString HoudiniAssetRelPath;
    if (!ResolveHoudiniAssetRelativePath(HoudiniAsset, HoudiniAssetRelPath))
    {
        // if we can't resolve a relative path for this HoudiniAsset tool, we can't apply category matching.
        return;
    }

    for(auto& Entry : InCategories)
    {
        const FString& CategoryName = Entry.Key;
        const FCategoryRules& Rules = Entry.Value;
        bool bExclude = false;

        if (!bIgnoreExclusionRules)
        {
            // Apply exclude rules
            for (const FString& Pattern : Rules.Exclude)
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
                continue;
            }
        }

        // Apply include rules
        for (const FString& Pattern : Rules.Include)
        {
            if (HoudiniAssetRelPath.MatchesWildcard(Pattern))
            {
                OutCategories.Add(CategoryName);
                break;
            }
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
    
	if ( FToolsWrapper::DoesAssetExist(AssetPath) )
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
                                   const UHoudiniToolsPackageAsset* InToolsPackage,
                                   bool bIgnoreToolData)
{
    if (!IsValid(InHoudiniAsset))
        return;
        
    if (!bIgnoreToolData && (IsValid(InHoudiniAsset->HoudiniToolData)))
    {
        // Populate from ToolData (this is our preferred data source) 
        UHoudiniToolData* ToolData = InHoudiniAsset->HoudiniToolData;
        HoudiniTool->HoudiniAsset = InHoudiniAsset;
        HoudiniTool->ToolsPackage = InToolsPackage;
        
        HoudiniTool->Name = FText::FromString(ToolData->Name);
        HoudiniTool->ToolTipText = FText::FromString(ToolData->ToolTip);
        // We don't really know what this HDA is, so we're just going with "no inputs" as a default.
        HoudiniTool->Type = ToolData->Type;
        HoudiniTool->SelectionType = ToolData->SelectionType;
        HoudiniTool->HelpURL = ToolData->HelpURL;
    }
    else
    {
        // Populate from any data we can extract from UHoudiniAsset without looking outside the Unreal project.
        // i.e., no peeking at external json files.
        HoudiniTool->HoudiniAsset = InHoudiniAsset;
        HoudiniTool->ToolsPackage = InToolsPackage;
        
        HoudiniTool->Name = FText::FromString(FPaths::GetBaseFilename(InHoudiniAsset->AssetFileName));
        HoudiniTool->ToolTipText = HoudiniTool->Name;
        // We don't really know what this HDA is, so we're just going with "no inputs" as a default.
        HoudiniTool->Type = EHoudiniToolType::HTOOLTYPE_GENERATOR;
        HoudiniTool->SelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
    }
    
    HoudiniTool->Icon = CreateUniqueAssetIconBrush(InHoudiniAsset);
}

bool
FHoudiniToolsEditor::FindHoudiniToolForAsset(UHoudiniAsset* HoudiniAsset, TSharedPtr<FHoudiniTool>& OutHoudiniTool)
{
    if (!IsValid(HoudiniAsset))
        return false;
    
    for (TSharedPtr<FHoudiniTool> ToolPtr : HoudiniTools)
    {
        if (ToolPtr.IsValid() && ToolPtr->HoudiniAsset == HoudiniAsset)
        {
            OutHoudiniTool = ToolPtr;
            return true;
        }
    }

    return false;
}

UHoudiniToolsPackageAsset* FHoudiniToolsEditor::LoadHoudiniToolsPackage(const FString& PackageBasePath)
{
    const FString PkgPath = FPaths::Combine(PackageBasePath, FString::Format(TEXT("{0}.{0}"), { FHoudiniToolsRuntimeUtils::GetPackageUAssetName() }) );
    return LoadObject<UHoudiniToolsPackageAsset>(nullptr, *PkgPath);
}

bool FHoudiniToolsEditor::ExcludeToolFromCategory(UHoudiniAsset* HoudiniAsset, const FString& CategoryName, bool bAddCategoryIfMissing)
{
    if (!IsValid(HoudiniAsset))
    {
        return false;
    }
    
    UHoudiniToolsPackageAsset* PackageAsset = FindOwningToolsPackage(HoudiniAsset);
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
    if (!ResolveHoudiniAssetRelativePath(HoudiniAsset, RelToolPath))
    {
        HOUDINI_LOG_WARNING(TEXT("Could not resolve tool path relative to an owning package: %s."), *HoudiniAsset->GetPathName());
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
        UE_LOG(LogHoudiniTools, Log, TEXT("[ExcludeToolFromCategory] Excluded '%s' from category '%s'."), *RelToolPath, *CategoryName);
    }

    PackageAsset->MarkPackageDirty();

    return true;
}

// void
// FHoudiniTools::GetHoudiniToolDirectories(const int32& SelectedDirIndex, TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray) const
// {
//     HoudiniToolsDirectoryArray.Empty();
//
//     // Get All the houdini tool directories
//     GetAllHoudiniToolDirectories(HoudiniToolsDirectoryArray);
//
//     // Only keep the selected one
//     // -1 indicates we want all directories
//     if ( SelectedDirIndex >= 0 )
//     {
//         for (int32 n = HoudiniToolsDirectoryArray.Num() - 1; n >= 0; n--)
//         {
//             if (n != CurrentHoudiniToolDirIndex)
//                 HoudiniToolsDirectoryArray.RemoveAt(n);
//         }
//     }
// }

// bool
// FHoudiniTools::FindHoudiniTool( const FHoudiniTool& Tool, int32& FoundIndex, bool& IsDefault )
// {
//     // Return -1 if we cant find the Tool
//     FoundIndex = -1;
//
//     for ( int32 Idx = 0; Idx < HoudiniTools.Num(); Idx++ )
//     {
//         TSharedPtr<FHoudiniTool> Current = HoudiniTools[ Idx ];
//         if ( !Current.IsValid() )
//             continue;
//
//         if ( Current->HoudiniAsset != Tool.HoudiniAsset )
//             continue;
//
//         if ( Current->Type != Tool.Type )
//             continue;
//
//         if ( Current->JSONFile != Tool.JSONFile )
//             continue;
//
//         if (Current->ToolDirectory != Tool.ToolDirectory)
//             continue;
//
//         // We found the Houdini Tool
//         IsDefault = Current->DefaultTool;
//         FoundIndex = Idx;
//
//         return true;
//     }
//
//     return false;
// }

void
FHoudiniToolsEditor::UpdateHoudiniToolListFromProject(bool bIgnoreExcludePatterns)
{
    // Clean up the existing tool list
    Categories.Empty();
    HoudiniTools.Empty();

    TArray<UHoudiniToolsPackageAsset*> ToolsPackages;
    FindHoudiniToolsPackages(ToolsPackages);
    
    for (const UHoudiniToolsPackageAsset* ToolsPackage : ToolsPackages)
    {
        // Find all tools in this package
        TArray<TSharedPtr<FHoudiniTool>> PackageTools;
        FindHoudiniToolsInPackage(ToolsPackage, PackageTools);

        // Apply category rules to package tools
        for(TSharedPtr<FHoudiniTool> HoudiniTool : PackageTools)
        {
            // Find the matching categories from the package 
            TArray<FString> MatchingCategories;
            ApplyCategories(ToolsPackage->Categories, HoudiniTool, bIgnoreExcludePatterns, MatchingCategories);

            // TODO: Apply user defined categories.

            // Add this tool to the respective category entries
            for(const FString& CategoryName : MatchingCategories)
            {
                TSharedPtr<FHoudiniToolList>& CategoryTools = Categories.FindOrAdd(CategoryName);
                if (!CategoryTools.IsValid())
                {
                    CategoryTools = MakeShared<FHoudiniToolList>();
                }

                CategoryTools->Tools.Add(HoudiniTool);
            }
        }

        // For now, ensure each category is sorted by HoudiniTool display name.
        // TODO: Implement a mechanism by which we can combine absolute order and wildcards/patterns.
        for(auto& Entry : Categories)
        {
            const TSharedPtr<FHoudiniToolList> CategoryTools = Entry.Value;
            CategoryTools->Tools.Sort([](const TSharedPtr<FHoudiniTool>& LHS, const TSharedPtr<FHoudiniTool>& RHS) -> bool
            {
                return LHS->Name.ToString() < RHS->Name.ToString();
            });
        }

        // Append the tools to our global package list.
        HoudiniTools.Append(PackageTools);
    }
    
    // // Get All the houdini tool directories
    // TArray<FHoudiniToolDirectory> HoudiniToolsDirectoryArray;
    // GetHoudiniToolDirectories( SelectedDir, HoudiniToolsDirectoryArray );

    // // Add the tools for all the directories
    // for ( int32 n = 0; n < HoudiniToolsDirectoryArray.Num(); n++ )
    // {
    //     FHoudiniToolDirectory CurrentDir = HoudiniToolsDirectoryArray[n];
    //     bool isDefault = ( (n == 0) && (CurrentHoudiniToolDirIndex <= 0) );
    //     UpdateHoudiniToolList( CurrentDir, isDefault );
    // }
}


void
FHoudiniToolsEditor::UpdatePackageToolListFromSource(UHoudiniToolsPackageAsset* ToolsPackage)
{
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
        
        UE_LOG(LogHoudiniTools, Log, TEXT("[GetHoudiniPackageDescriptionFromJSON] Trying to read categories string array field: %d"), CategoryEntries.Num());
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

            UE_LOG(LogHoudiniTools, Log, TEXT("[GetHoudiniPackageDescriptionFromJSON] Category '%s': includes %d, excludes, %d"), *CategoryName, OutCategoryRules.Include.Num(), OutCategoryRules.Exclude.Num());
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

    // if (HDAFilePath.IsEmpty())
    // {
    //     // Look for an hda file with the same name as the json file
    //     HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hda"));
    //     if (!FPaths::FileExists(HDAFilePath))
    //     {
    //         // Try .hdalc
    //         HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdalc"));
    //         if (!FPaths::FileExists(HDAFilePath))
    //         {
    //             // Try .hdanc
    //             HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdanc"));
    //             if (!FPaths::FileExists(HDAFilePath))
    //                 HDAFilePath = FString();
    //         }
    //     }
    // }
    //
    // if ( HDAFilePath.IsEmpty() )
    // {
    //     HOUDINI_LOG_ERROR(TEXT("Could not find the hda for the Houdini Tool .json file: %s"), *JsonFilePath);
    //     return false;
    // }
    //
    // // Create a new asset.
    // OutAssetPath = FFilePath{ HDAFilePath };
    //
    // // Read the tool name
    // OutName = FText::FromString(FPaths::GetBaseFilename(JsonFilePath));
    // if ( JSONObject->HasField(TEXT("name") ) )
    //     OutName = FText::FromString(JSONObject->GetStringField(TEXT("name")));
    //
    // // Read the tool type
    // OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
    // if ( JSONObject->HasField( TEXT("toolType") ) )
    // {
    //     FString ToolType = JSONObject->GetStringField(TEXT("toolType"));
    //     if ( ToolType.Equals( TEXT("GENERATOR"), ESearchCase::IgnoreCase ) )
    //         OutType = EHoudiniToolType::HTOOLTYPE_GENERATOR;
    //     else if (ToolType.Equals( TEXT("OPERATOR_SINGLE"), ESearchCase::IgnoreCase ) )
    //         OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
    //     else if (ToolType.Equals(TEXT("OPERATOR_MULTI"), ESearchCase::IgnoreCase))
    //         OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI;
    //     else if (ToolType.Equals(TEXT("BATCH"), ESearchCase::IgnoreCase))
    //         OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH;
    // }
    //
    // // Read the tooltip
    // OutToolTip = FText();
    // if ( JSONObject->HasField( TEXT("toolTip") ) )
    // {
    //     OutToolTip =  FText::FromString(JSONObject->GetStringField(TEXT("toolTip")));
    // }
    //
    // // Read the help url
    // OutHelpURL = FString();
    // if (JSONObject->HasField(TEXT("helpURL")))
    // {
    //     OutHelpURL = JSONObject->GetStringField(TEXT("helpURL"));
    // }
    //
    // // Read the icon path
    // FString IconPath = FString();
    // if (JSONObject->HasField(TEXT("iconPath")))
    // {
    //     // If the json has the iconPath field, read it from there
    //     IconPath = JSONObject->GetStringField(TEXT("iconPath"));
    //     if (!FPaths::FileExists(IconPath))
    //         IconPath = FString();
    // }
    //
    // if (IconPath.IsEmpty())
    // {
    //     // Look for a png file with the same name as the json file
    //     IconPath = JsonFilePath.Replace(TEXT(".json"), TEXT(".png"));
    //     if (!FPaths::FileExists(IconPath))
    //     {
    //         IconPath = GetDefaultHoudiniToolIcon();
    //     }
    // }
    //
    // return true;
}

bool FHoudiniToolsEditor::ImportExternalHoudiniAssetData(UHoudiniAsset* HoudiniAsset)
{
    HOUDINI_LOG_MESSAGE(TEXT("[ImportExternalHoudiniAssetData] ..."));
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
	    UE_LOG(LogHoudiniTools, Log, TEXT("[ImportExternalHoudiniAssetData] Populating ToolsData from JSON file: %s"), *JSONFilePath);
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
        UE_LOG(LogHoudiniTools, Log, TEXT("[ImportExternalHoudiniAssetData] Importing Icon: %s"), *IconPath);
        // If we have found an icon for this HDA, load it now.
        UHoudiniToolData* ToolData = GetOrCreateHoudiniToolData(HoudiniAsset);
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
        TArray<TSharedPtr<FJsonValue> >TargetArray = JSONObject->GetArrayField("target");
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
            IconPath = FString();
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
        if (FToolsWrapper::DoesAssetExist(PackageAssetPath))
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
    
    Asset->ExternalPackageDir.Path = PackageJsonPath;
    
    return Asset;
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

    UE_LOG(LogHoudiniTools, Log, TEXT("Finding HDAs in external package dir: %s"), *ExternalPackageDir);
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
        TSoftObjectPtr<UHoudiniAsset> HoudiniAssetRef(ToolAssetRefPath);
        UHoudiniAsset* LoadedAsset = nullptr;

        //
        // See if the HDA needs to be imported in Unreal, or just loaded
        bool bHDAExists = false;
        if ( !HoudiniAssetRef.IsValid() )
        {
            // Try to load the asset, if it exists.
            if (FToolsWrapper::DoesAssetExist(ToolPath))
            {
                // Try to load the asset
                LoadedAsset = HoudiniAssetRef.LoadSynchronous();
                if ( IsValid(LoadedAsset) )
                    bHDAExists = !HoudiniAssetRef.IsValid();
            }
        }
        else
        {
            bHDAExists = true;
        }

        if (bHDAExists && bOnlyMissing)
        {
            // We're only interesting in missing HDAs
            continue;
        }

        if (!bHDAExists)
        {
            // We check an edge case here where an asset might exist at the toolpath, but it's not an HoudiniAsset.
            if (FToolsWrapper::DoesAssetExist(ToolPath))
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
            FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
            TArray<FString> FileNames;
            FString RelImportPath = ExternalHDAFile;
            FPaths::MakePathRelativeTo(RelImportPath, *GetAbsoluteGameContentDir());
            
            InternalHoudiniAssets.Remove(LoadedAsset);

            FToolsWrapper::ReimportAsync(LoadedAsset, false, true, RelImportPath);
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

        // reimport any HDAs left over in this array 
        for (UHoudiniAsset* HoudiniAsset : InternalHoudiniAssets)
        {
            UE_LOG(LogHoudiniTools, Log, TEXT("[FHoudiniTools::ReimportPackageHDAs] Reimporting internal Houdini Asset: %s"), *HoudiniAsset->GetPathName());
            FToolsWrapper::ReimportAsync(HoudiniAsset, false, true);
        }
    }
    
    return false;
}

const FSlateBrush*
FHoudiniToolsEditor::CreateUniqueAssetIconBrush(const UHoudiniAsset* HoudiniAsset)
{
    if (IsValid(HoudiniAsset) && IsValid(HoudiniAsset->HoudiniToolData))
    {
        UE_LOG(LogHoudiniTools, Log, TEXT("[FHoudiniToolsEditor::CreateUniqueAssetIconBrush] '%s' Icon '%s' (%s)"), *(HoudiniAsset->HoudiniToolData->Name), *(HoudiniAsset->HoudiniToolData->IconSourcePath.FilePath), *(HoudiniAsset->HoudiniToolData->IconImageData.RawDataMD5));
        const FHImageData& SrcImageData = HoudiniAsset->HoudiniToolData->IconImageData;

        if (SrcImageData.SizeX == 0 || SrcImageData.SizeY == 0 || SrcImageData.RawData.Num() == 0)
        {
            return nullptr;
        }

        FImage SrcImage;
        SrcImageData.ToImage(SrcImage);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
        if (!SrcImage.IsImageInfoValid())
        {
            return nullptr;
        }
#endif
        
        const FString ResourceName = HoudiniAsset->AssetFileName;
        
        // We have a TArray64 but API needs a TArray
		const TArray<uint8> ImageRawData32( MoveTemp(SrcImage.RawData) );
        // Append the hash of the image data. This will allow the icon to create a unique resource
        // for different image data, even if the icon image path is the same. At the same time, we're
        // able to reuse image resource that haven't changed.
        const FString UniqueResourceName = ResourceName + "_" + SrcImageData.RawDataMD5;
		if (FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(*UniqueResourceName, SrcImage.SizeX, SrcImage.SizeY, ImageRawData32))
		{
            UE_LOG(LogHoudiniTools, Log, TEXT("[FHoudiniToolsEditor::CreateUniqueAssetIconBrush] Created icon brush with resource name: %s"), *UniqueResourceName);
			return new FSlateDynamicImageBrush(*UniqueResourceName, FVector2D(40.f, 40.f));
		}
    }
    
    return nullptr;
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

    FString AssetPath = FPaths::GetPath(ToolsPackage->GetPathName());
    UE_LOG(LogHoudiniTools, Log, TEXT("[MakeDefaultJSONObject] Asset Path: %s"), *AssetPath);

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

// void
// FHoudiniTools::UpdateHoudiniToolList_DEPR(const FHoudiniToolDirectory& HoudiniToolsDirectory, const bool& isDefault)
// {
//     FString ToolDirPath = HoudiniToolsDirectory.Path.Path;
//     if ( ToolDirPath.IsEmpty() )
//         return;
//
//     // We need to either load or create a new HoudiniAsset from the tool's HDA
//     FString ToolPath = GetDefaultHoudiniToolsPath();
//     ToolPath = FPaths::Combine(ToolPath, HoudiniToolsDirectory.ContentDirID );
//     ToolPath = ObjectTools::SanitizeObjectPath(ToolPath);
//
//     // List all the json files in the current directory
//     TArray<FString> JSONFiles;
//     IFileManager::Get().FindFiles(JSONFiles, *ToolDirPath, TEXT(".json"));
//     for ( const FString& CurrentJsonFile : JSONFiles )
//     {
//         FText CurrentToolName;
//         EHoudiniToolType CurrentToolType;
//         EHoudiniToolSelectionType CurrentToolSelectionType;
//         FText CurrentToolToolTip;
//         FFilePath CurrentToolIconPath;
//         FFilePath CurrentToolAssetPath;
//         FString CurrentToolHelpURL;
//
//         // Extract the Tool info from the JSON file
//         FString CurrentJsonFilePath = ToolDirPath / CurrentJsonFile;
//         if ( !GetHoudiniToolDescriptionFromJSON(
//             CurrentJsonFilePath, CurrentToolName, CurrentToolType, CurrentToolSelectionType,
//             CurrentToolToolTip, CurrentToolIconPath, CurrentToolAssetPath, CurrentToolHelpURL))
//             continue;
//
//         FText ToolName = CurrentToolName;
//         FText ToolTip = CurrentToolToolTip;
//
//         FString IconPath = FPaths::ConvertRelativePathToFull(CurrentToolIconPath.FilePath);
//         const FSlateBrush* CustomIconBrush = nullptr;
//         if (FPaths::FileExists(IconPath))
//         {
//             FName BrushName = *IconPath;
//             CustomIconBrush = new FSlateDynamicImageBrush(BrushName, FVector2D(40.f, 40.f));
//         }
//         else
//         {
//             CustomIconBrush = FHoudiniEngineStyle::Get()->GetBrush(TEXT("HoudiniEngine.HoudiniEngineLogo40"));
//         }
//
//         // Construct the asset's ref
//         FString BaseToolName = ObjectTools::SanitizeObjectName( FPaths::GetBaseFilename( CurrentToolAssetPath.FilePath ) );
//         FString ToolAssetRef = TEXT("HoudiniAsset'") + FPaths::Combine(ToolPath, BaseToolName) + TEXT(".") + BaseToolName + TEXT("'");
//         TSoftObjectPtr<UHoudiniAsset> HoudiniAsset(ToolAssetRef);
//
//         // See if the HDA needs to be imported in Unreal, or just loaded
//         bool NeedsImport = false;
//         if ( !HoudiniAsset.IsValid() )
//         {
//             NeedsImport = true;
//
//             // Try to load the asset
//             UHoudiniAsset* LoadedAsset = HoudiniAsset.LoadSynchronous();
//             if ( IsValid(LoadedAsset) )
//                 NeedsImport = !HoudiniAsset.IsValid();
//         }
//
//         if ( NeedsImport )
//         {
//             // Automatically import the HDA and create a uasset for it
//             FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
//             TArray<FString> FileNames;
//             FileNames.Add(CurrentToolAssetPath.FilePath);
//
//             UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
//             ImportData->bReplaceExisting = true;
//             ImportData->bSkipReadOnly = true;
//             ImportData->Filenames = FileNames;
//             ImportData->DestinationPath = ToolPath;
//             TArray<UObject*> AssetArray = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
//             if ( AssetArray.Num() <=  0 )
//                 continue;
//
//             HoudiniAsset = Cast< UHoudiniAsset >(AssetArray[0]);
//             if (!HoudiniAsset.IsValid() )
//                 continue;
//
//             // Try to save the newly created package
//             UPackage* Pckg = Cast<UPackage>( HoudiniAsset->GetOuter() );
//             if (IsValid(Pckg) && Pckg->IsDirty() )
//             {
//                 Pckg->FullyLoad();
//                 UPackage::SavePackage(
//                     Pckg, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
//                     *FPackageName::LongPackageNameToFilename( Pckg->GetName(), FPackageName::GetAssetPackageExtension() ) );
//             }
//         }
//
//         // Add the tool to the tool list
//         HoudiniTools.Add( MakeShareable( new FHoudiniTool(
//             HoudiniAsset, ToolName, CurrentToolType, CurrentToolSelectionType, ToolTip, CustomIconBrush, CurrentToolHelpURL, isDefault, CurrentToolAssetPath, HoudiniToolsDirectory, CurrentJsonFile ) ) );
//     }
// }



bool
FHoudiniToolsEditor::WriteJSONFromHoudiniTool_DEPR(const FHoudiniTool& Tool)
{
    // Start by building a JSON object from the tool
    TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject);

    // Mark the target as unreal only
    TArray< TSharedPtr<FJsonValue> > TargetValue;
    TargetValue.Add(MakeShareable(new FJsonValueString(TEXT("unreal"))));
    JSONObject->SetArrayField(TEXT("target"), TargetValue );

    // Write the asset Path
    if (!Tool.SourceAssetPath.FilePath.IsEmpty())
    {
        // Only write the assetPath if it's different from the default one (same path as JSON)
        if (FPaths::GetBaseFilename(Tool.SourceAssetPath.FilePath) != (FPaths::GetBaseFilename(Tool.JSONFile))
            && (FPaths::GetPath(Tool.SourceAssetPath.FilePath) != Tool.ToolDirectory.Path.Path))
        {
            JSONObject->SetStringField(TEXT("assetPath"), FPaths::ConvertRelativePathToFull(Tool.SourceAssetPath.FilePath));
        }
    }

    // The Tool Name
    if ( !Tool.Name.IsEmpty() )
        JSONObject->SetStringField(TEXT("name"), Tool.Name.ToString());

    // Tooltype
    FString ToolType = TEXT("GENERATOR");
    if ( Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
        ToolType = TEXT("OPERATOR_SINGLE");
    else if (Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI)
        ToolType = TEXT("OPERATOR_MULTI");
    else if ( Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH)
        ToolType = TEXT("BATCH");

    JSONObject->SetStringField(TEXT("toolType"), ToolType);

    // Tooltip
    if ( !Tool.ToolTipText.IsEmpty() )
        JSONObject->SetStringField(TEXT("toolTip"), Tool.ToolTipText.ToString());

    // Help URL
    if ( !Tool.HelpURL.IsEmpty() )
        JSONObject->SetStringField(TEXT("helpURL"), Tool.HelpURL);

    // IconPath
    if ( Tool.Icon )
    {
        FString IconPath = Tool.Icon->GetResourceName().ToString();
        JSONObject->SetStringField(TEXT("iconPath"), IconPath);
    }

    // Selection Type
    FString SelectionType = TEXT("All");
    if (Tool.SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY)
        SelectionType = TEXT("CB");
    else if (Tool.SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY)
        SelectionType = TEXT("World");
    JSONObject->SetStringField(TEXT("UE_SelectionType"), SelectionType);

    FString ToolJSONFilePath = Tool.ToolDirectory.Path.Path / Tool.JSONFile;

    // Output the JSON to a String
    FString OutputString;
    TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JSONObject.ToSharedRef(), Writer);

    // Then write the output string to the json file itself
    FString SaveDirectory = Tool.ToolDirectory.Path.Path;
    FString FileName = Tool.JSONFile;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    // Returns true if the directory existed or has been created
    if ( PlatformFile.CreateDirectoryTree(*SaveDirectory) )
    {        
        FString AbsoluteFilePath = SaveDirectory / FileName;
        return FFileHelper::SaveStringToFile(OutputString, *AbsoluteFilePath);
    }

    return false;
}

void FHoudiniToolsEditor::BroadcastToolChanged()
{
    OnToolChanged.Broadcast();
}


