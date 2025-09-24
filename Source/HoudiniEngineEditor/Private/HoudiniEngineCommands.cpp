/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniEngineCommands.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniAssetActor.h"
#include "HoudiniCookable.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniOutput.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngineDetails.h"
#include "UnrealObjectInputManager.h"

#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "EditorDirectories.h"
#include "Misc/ScopedSlowTask.h"
#include "Async/Async.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"
#include "ObjectTools.h"
#include "CoreGlobals.h"
#include "HoudiniEngineOutputStats.h"
#include "Misc/FeedbackContext.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "UObject/ObjectSaveContext.h"
//#include "UObject/ObjectSaveContext.h"
#include "LevelEditor.h"
#include "UObject/UObjectIterator.h"
#if defined(HOUDINI_USE_PCG)
#include "HoudiniPCGUtils.h"
#endif
#include "Trace/StoreClient.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

FHoudiniEngineCommands::FHoudiniEngineCommands()
	: TCommands<FHoudiniEngineCommands>	(TEXT("HoudiniEngine"), NSLOCTEXT("Contexts", "HoudiniEngine", "Houdini Engine Plugin"), NAME_None, FHoudiniEngineStyle::GetStyleSetName())
{
}

void
FHoudiniEngineCommands::RegisterCommands()
{	
	UI_COMMAND(_CreateSession, "Create Session", "Creates a new Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_ConnectSession, "Connect Session", "Connects to an existing Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_StopSession, "Stop Session", "Stops the current Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_RestartSession, "Restart Session", "Restarts the current Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_OpenSessionSync, "Open Houdini Session Sync...", "Opens Houdini with Session Sync and connect to it.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_CloseSessionSync, "Close Houdini Session Sync", "Close the Session Sync Houdini.", EUserInterfaceActionType::Button, FInputChord());

	// Viewport Sync
	UI_COMMAND(_ViewportSyncNone, "Disabled", "Do not sync viewports.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncUnreal, "Sync Unreal to Houdini.", "Sync the Unreal viewport to Houdini's.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncHoudini, "Sync Houdini to Unreal", "Sync the Houdini viewport to Unreal's.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncBoth, "Both", "Sync both Unreal and Houdini's viewport.", EUserInterfaceActionType::Check, FInputChord());

	//NodeSync
	UI_COMMAND(_OpenNodeSync, "Houdini Node Sync...", "Opens the Houdini Node Sync Panel.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_OpenHoudiniTools, "Houdini Tools...", "Opens the Houdini Tools Panel.", EUserInterfaceActionType::Button, FInputChord());

	// PDG Import Commandlet
	UI_COMMAND(_StartPDGCommandlet, "Start Async Importer", "Start the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_StopPDGCommandlet, "Stop Async Importer", "Stops the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_IsPDGCommandletEnabled, "Enable Async Importer", "Enables the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Check, FInputChord());
	
	UI_COMMAND(_InstallInfo, "Installation Info...", "Display information on the current Houdini Engine installation", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_PluginSettings, "Plugin Settings...", "Displays the Houdini Engine plugin project settings", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_PluginEditorSettings, "Plugin Editor Preferences...", "Displays the Houdini Engine plugin editor preferences", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(_OpenInHoudini, "Open scene in Houdini...", "Opens the current Houdini scene in Houdini.", EUserInterfaceActionType::Button, FInputChord(EKeys::O, EModifierKey::Control | EModifierKey::Alt));
	UI_COMMAND(_SaveHIPFile, "Save Houdini scene (HIP)", "Saves a .hip file of the current Houdini scene.", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(_ContentExampleGit, "Content Example...", "Opens the GitHub repository that contains the plugin's content examples.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_ContentExampleBrowseTo, "Browse Content Examples...", "Browse to the installed content example folder in the current project (if installed).", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(_OnlineDoc, "Online Documentation...", "Go to the plugin's online documentation.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_OnlineForum, "Online Forum...", "Go to the plugin's online forum.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_ReportBug, "Report a bug...", "Report a bug for Houdini Engine for Unreal plugin.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(_CookAll, "Recook All", "Recooks all Houdini Assets Actors in the current level.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_CookSelected, "Recook Selection", "Recooks selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control | EModifierKey::Alt));

	UI_COMMAND(_RebuildAll, "Rebuild All", "Rebuilds all Houdini Assets Actors in the current level.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_RebuildSelected, "Rebuild Selection", "Rebuilds selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord(EKeys::R, EModifierKey::Control | EModifierKey::Alt));

	UI_COMMAND(_BakeAll, "Bake And Replace All Houdini Assets", "Bakes and replaces with blueprints all Houdini Assets in the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_BakeSelected, "Bake And Replace Selection", "Bakes and replaces with blueprints selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord(EKeys::B, EModifierKey::Control | EModifierKey::Alt));

	UI_COMMAND(_RefineAll, "Refine all Houdini Proxy Meshes To Static Meshes", "Builds and replaces all Houdini proxy meshes with UStaticMesh instances.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_RefineSelected, "Refine selected Houdini Proxy Meshes To Static Meshes", "Builds and replaces selected Houdini proxy meshes with UStaticMesh instances.", EUserInterfaceActionType::Button, FInputChord());	

	UI_COMMAND(_CleanUpTempFolder, "Clean Houdini Engine Temp Folder", "Deletes the unused temporary files in the Temporary Cook Folder.", EUserInterfaceActionType::Button, FInputChord());	
	UI_COMMAND(_PauseAssetCooking, "Pause Houdini Engine Cooking", "When activated, prevents Houdini Engine from cooking assets until unpaused.", EUserInterfaceActionType::Check, FInputChord(EKeys::P, EModifierKey::Control | EModifierKey::Alt));
}

void
FHoudiniEngineCommands::SaveHIPFile()
{
	if (!FHoudiniEngine::IsInitialized() || FHoudiniEngine::Get().GetSession() == nullptr)
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot save the Houdini scene, the Houdini Engine session hasn't been started."));
		return;
	}

	IDesktopPlatform * DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform || !FHoudiniEngineUtils::IsInitialized())
		return;

	TArray<FString> SaveFilenames;
	bool bSaved = false;
	void * ParentWindowWindowHandle = NULL;

	IMainFrameModule & MainFrameModule = FModuleManager::LoadModuleChecked< IMainFrameModule >(TEXT("MainFrame"));
	const TSharedPtr< SWindow > & MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();

	FString FileType = TEXT("Houdini HIP file|*.hip");
	if(FHoudiniEngine::Get().IsLicenseIndie())
		FileType = TEXT("Houdini HIP file (Limited Commerical)|*.hiplc");
	else if(FHoudiniEngine::Get().IsLicenseEducation())
		FileType = TEXT("Houdini HIP file (Non Commercial)|*.hipnc");

	bSaved = DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		NSLOCTEXT("SaveHIPFile", "SaveHIPFile", "Saves a .hip file of the current Houdini scene.").ToString(),
		*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
		TEXT(""),
		FileType,
		EFileDialogFlags::None,
		SaveFilenames);

	if(bSaved && SaveFilenames.Num())
	{
		// Add a slate notification
		FString Notification = TEXT("Saving internal Houdini scene...");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		// ... and a log message
		HOUDINI_LOG_MESSAGE(TEXT("Saved Houdini scene to %s"), *SaveFilenames[0]);

		// Get first path.
		std::string HIPPathConverted(H_TCHAR_TO_UTF8(*SaveFilenames[0]));

		// Save HIP file through Engine.
		FHoudiniApi::SaveHIPFile(FHoudiniEngine::Get().GetSession(), HIPPathConverted.c_str(), false);
	}
}

void
FHoudiniEngineCommands::OpenInHoudini()
{
	if(!FHoudiniEngine::IsInitialized() || FHoudiniEngine::Get().GetSession() == nullptr)
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot open the scene in Houdini, the Houdini Engine session hasn't been started."));
		return;
	}

	FString FileExtension = TEXT(".hip");
	if (FHoudiniEngine::Get().IsLicenseIndie())
		FileExtension = TEXT(".hiplc");
	else if (FHoudiniEngine::Get().IsLicenseEducation())
		FileExtension = TEXT(".hipnc");

	// First, saves the current scene as a hip file
	// Creates a proper temporary file name
	FString UserTempPath = FPaths::CreateTempFilename(
		FPlatformProcess::UserTempDir(),
		TEXT("HoudiniEngine"), *FileExtension);

	// Save HIP file through Engine.
	std::string TempPathConverted(H_TCHAR_TO_UTF8(*UserTempPath));
	FHoudiniApi::SaveHIPFile(
		FHoudiniEngine::Get().GetSession(),
		TempPathConverted.c_str(), false);

	if (!FPaths::FileExists(UserTempPath))
		return;

	// Add a slate notification
	FString Notification = TEXT("Opening scene in Houdini...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Add quotes to the path to avoid issues with spaces
	UserTempPath = TEXT("\"") + UserTempPath + TEXT("\"");

	// Set custom $HOME env var if it's been specified
	FHoudiniEngineRuntimeUtils::SetHoudiniHomeEnvironmentVariable();

	// Then open the hip file in Houdini
	FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
	FString HoudiniExecutable = FHoudiniEngine::Get().GetHoudiniExecutable();
	FString HoudiniLocation = LibHAPILocation + TEXT("//") + HoudiniExecutable;

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*HoudiniLocation,
		*UserTempPath,
		true, false, false,
		nullptr, 0,
		*FPlatformProcess::GetCurrentWorkingDirectory(),
		nullptr, nullptr);

	if (!ProcHandle.IsValid())
	{
		// Try with the steam version executable instead
		HoudiniLocation = LibHAPILocation + TEXT("//hindie.steam");

		ProcHandle = FPlatformProcess::CreateProc(
			*HoudiniLocation,
			*UserTempPath,
			true, false, false,
			nullptr, 0,
			*FPlatformProcess::GetCurrentWorkingDirectory(),
			nullptr, nullptr);

		if (!ProcHandle.IsValid())
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to open scene in Houdini."));
		}
	}

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Opened scene in Houdini."));
}

void
FHoudiniEngineCommands::ReportBug()
{
	FPlatformProcess::LaunchURL(HAPI_UNREAL_BUG_REPORT_URL, nullptr, nullptr);
}

void
FHoudiniEngineCommands::ShowInstallInfo()
{
	FHoudiniEngineDetails::CreateInstallInfoWindow();
}

void
FHoudiniEngineCommands::ShowPluginSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("HoudiniEngine"));
}

void
FHoudiniEngineCommands::ShowPluginEditorSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Editor"), FName("Plugins"), FName("HoudiniEngine"));
}

void
FHoudiniEngineCommands::OpenContentExampleGit()
{
	FPlatformProcess::LaunchURL(HAPI_UNREAL_CONTENT_EXAMPLES_URL, nullptr, nullptr);
}

void
FHoudiniEngineCommands::BrowseToContentExamples()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngineExamples"));
	if (!Plugin.IsValid())// || !Plugin->IsEnabled())
		return;

	// Get the ContentExample's folder
	//FString CEFolder = Plugin->GetContentDir() + "/ContentExamples/Maps";
	FString CEFolder = "/HoudiniEngineExamples/ContentExamples/Maps";

	TArray<FString> FolderList;
	FolderList.Push(CEFolder);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().FocusPrimaryContentBrowser(false);
	ContentBrowserModule.Get().ForceShowPluginContent(true);
	//ContentBrowserModule.Get().SetSelectedPaths(FolderList, true);
	ContentBrowserModule.Get().SyncBrowserToFolders(FolderList, true, true);
}

bool
FHoudiniEngineCommands::HasContentExamples()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngineExamples"));
	if (!Plugin.IsValid())
		return false;
	
	if (!Plugin->IsEnabled())
		return false;

	return true;
}

void
FHoudiniEngineCommands::OnlineDocumentation()
{
	FPlatformProcess::LaunchURL(HAPI_UNREAL_ONLINE_DOC_URL, nullptr, nullptr);
}

void
FHoudiniEngineCommands::OnlineForum()
{
	FPlatformProcess::LaunchURL(HAPI_UNREAL_ONLINE_FORUM_URL, nullptr, nullptr);
}

void
FHoudiniEngineCommands::CleanUpTempFolder()
{
	// TODO: Improve me! slow now that we also have SM saved in the temp directory
	// Due to the ref, we probably iterate a little too much, and should maybe do passes following the order of refs:
	// mesh first, then materials, then textures.
	// have a look at UWrangleContentCommandlet as well

	// Add a slate notification
	FString Notification = TEXT("Cleaning up Houdini Engine temporary folder...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	GWarn->BeginSlowTask(LOCTEXT("CleanUpTemp", "Cleaning up the Houdini Engine Temp Folder"), false, false);

	// Get the default temp cook folder
	FString TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

	TArray<FString> TempCookFolders;
	TempCookFolders.Add(FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder());
	for (TObjectIterator<UHoudiniCookable> It; It; ++It)
	{
		FString CookFolder = It->GetTemporaryCookFolder().Path;
		if (CookFolder.IsEmpty())
			continue;

		TempCookFolders.AddUnique(CookFolder);
	}

	// The Asset registry will help us finding if the content of the asset is referenced
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	int32 DeletedCount = 0;
	bool bDidDeleteAsset = true;
	while (bDidDeleteAsset)
	{
		// To correctly clean the temp folder, we need to iterate multiple times, because some of the temp assets
		// might be referenced by other temp assets.. (ie Textures are referenced by Materials)
		// We'll stop looking for assets to delete when no deletion occured.
		bDidDeleteAsset = false;

		TArray<FAssetData> AssetDataList;
		for (auto& TempFolder : TempCookFolders)
		{
			// The Object library will list all UObjects found in the TempFolder
			auto ObjectLibrary = UObjectLibrary::CreateLibrary(UObject::StaticClass(), false, true);
			ObjectLibrary->LoadAssetDataFromPath(TempFolder);

			// Get all the assets found in the TEMP folder
			TArray<FAssetData> CurrentAssetDataList;
			ObjectLibrary->GetAssetDataList(CurrentAssetDataList);

			AssetDataList.Append(CurrentAssetDataList);
		}

		// All the assets we're going to delete
		TArray<FAssetData> AssetDataToDelete;
		for (FAssetData Data : AssetDataList)
		{
			UPackage* CurrentPackage = Data.GetPackage();
			if (!IsValid(CurrentPackage))
				continue;

			// Do not  try to delete the package if it's referenced anywhere
			TArray<FName> ReferenceNames;
			AssetRegistryModule.Get().GetReferencers(CurrentPackage->GetFName(), ReferenceNames, UE::AssetRegistry::EDependencyCategory::All);

			if (ReferenceNames.Num() > 0)
				continue;

			bool bAssetDataSafeToDelete = true;
			TArray<FAssetData> AssetsInPackage;
			AssetRegistryModule.Get().GetAssetsByPackageName(CurrentPackage->GetFName(), AssetsInPackage);
			for (const auto& AssetInfo : AssetsInPackage)
			{
				// Check if the objects contained in the package are referenced by something that won't be garbage collected (*including* the undo buffer)                    
				UObject* AssetInPackage = AssetInfo.GetAsset();
				if (!IsValid(AssetInPackage))
					continue;

				FReferencerInformationList ReferencesIncludingUndo;
				bool bReferencedInMemoryOrUndoStack = IsReferenced(
					AssetInPackage,
					GARBAGE_COLLECTION_KEEPFLAGS,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
					EInternalObjectFlags_GarbageCollectionKeepFlags,
#else
					EInternalObjectFlags::GarbageCollectionKeepFlags,
#endif
					true,
					&ReferencesIncludingUndo);

				if (!bReferencedInMemoryOrUndoStack)
					continue;

				// We do have external references, check if the external references are in our ObjectToDelete list
				// If they are, we can delete the asset because its references are going to be deleted as well.
				for (auto ExtRef : ReferencesIncludingUndo.ExternalReferences)
				{
					UObject* Outer = ExtRef.Referencer->GetOuter();
					if (!IsValid(Outer))
						continue;

					bool bOuterFound = false;
					for (auto DataToDelete : AssetDataToDelete)
					{
						if (DataToDelete.GetPackage() == Outer)
						{
							bOuterFound = true;
							break;
						}
						else if (DataToDelete.GetAsset() == Outer)
						{
							bOuterFound = true;
							break;
						}
					}

					// We have at least one reference that's not going to be deleted, we have to keep the asset
					if (!bOuterFound)
					{
						bAssetDataSafeToDelete = false;
						break;
					}
				}
			}

			if (bAssetDataSafeToDelete)
				AssetDataToDelete.Add(Data);
		}

		// Nothing to delete
		if (AssetDataToDelete.Num() <= 0)
			break;

		int32 CurrentDeleted = ObjectTools::DeleteAssets(AssetDataToDelete, false);

		if (CurrentDeleted > 0)
		{
			DeletedCount += CurrentDeleted;
			bDidDeleteAsset = true;
		}
	}


	// Now, go through all the directories in the temp directories and delete all the empty ones
	IFileManager& FM = IFileManager::Get();
	// Lambda that parses a directory recursively and returns true if it is empty
	auto IsEmptyFolder = [&FM](FString PathToDeleteOnDisk)
	{
		struct FEmptyFolderVisitor : public IPlatformFile::FDirectoryVisitor
		{
			bool bIsEmpty;
			FEmptyFolderVisitor()
				: bIsEmpty(true)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (!bIsDirectory)
				{
					bIsEmpty = false;
					return false; // abort searching
				}

				return true; // continue searching
			}
		};

		// Look for files on disk in case the folder contains things not tracked by the asset registry
		FEmptyFolderVisitor EmptyFolderVisitor;
		IFileManager::Get().IterateDirectoryRecursively(*PathToDeleteOnDisk, EmptyFolderVisitor);
		return EmptyFolderVisitor.bIsEmpty;
	};
	
	// Iterates on all the temporary cook directories recursively,
	// And keep not of all the empty directories
	FString TempCookPathOnDisk;
	TArray<FString> FoldersToDelete;
	if (FPackageName::TryConvertLongPackageNameToFilename(TempCookFolder, TempCookPathOnDisk))
	{
		FM.IterateDirectoryRecursively(*TempCookPathOnDisk, [&FM, &FoldersToDelete, &IsEmptyFolder](const TCHAR* InFilenameOrDirectory, const bool InIsDirectory) -> bool
		{
			// Skip Files
			if (!InIsDirectory)
				return true;

			FString CurrentDirectoryPath = FString(InFilenameOrDirectory);
			if (IsEmptyFolder(CurrentDirectoryPath))
				FoldersToDelete.Add(CurrentDirectoryPath);

			// keep iterating
			return true;
		});
	}

	int32 DeletedDirectories = 0;
	for (auto& FolderPath : FoldersToDelete)
	{
		FString PathToDelete;
		if (!FPackageName::TryConvertFilenameToLongPackageName(FolderPath, PathToDelete))
			continue;

		if (IFileManager::Get().DeleteDirectory(*FolderPath, false, true))
		{
			AssetRegistryModule.Get().RemovePath(PathToDelete);
			DeletedDirectories++;
		}
	}

	GWarn->EndSlowTask();

	// Add a slate notification
	Notification = TEXT("Deleted ") + FString::FromInt(DeletedCount) + TEXT(" temporary files and ") + FString::FromInt(DeletedDirectories) + TEXT(" directories.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Deleted %d temporary files and %d directories."), DeletedCount, DeletedDirectories);
}

void
FHoudiniEngineCommands::BakeAllAssets()
{
	// Add a slate notification
	FString Notification = TEXT("Baking all assets in the current level...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Bakes and replaces with blueprints all Houdini Assets in the current level
	int32 BakedCount = 0;
	for (TObjectIterator<UHoudiniCookable> Itr; Itr; ++Itr)
	{
		UHoudiniCookable * CurrentHC = *Itr;
		if (!IsValid(CurrentHC))
		{
			FString AssetName = CurrentHC->GetOuter() ? CurrentHC->GetOuter()->GetName() : CurrentHC->GetName();
			if (AssetName != "Default__HoudiniAssetActor")
				HOUDINI_LOG_ERROR(TEXT("Failed to bake a Houdini Asset in the scene! -  %s is invalid"), *AssetName);
			continue;
		}

		// If component is not cooking or instancing, we can bake blueprint.
		if (CurrentHC->IsInstantiatingOrCooking())
		{
			FString AssetName = CurrentHC->GetOuter() ? CurrentHC->GetOuter()->GetName() : CurrentHC->GetName();
			HOUDINI_LOG_ERROR(TEXT("Failed to bake a Houdini Asset in the scene! -  %s is actively instantiating or cooking"), *AssetName);
			continue;
		}

		bool bSuccess = false;
		bool BakeToBlueprints = true;
		if (BakeToBlueprints)
		{
			FHoudiniBakedObjectData BakeOutputs;
			FHoudiniBakeSettings BakeOptions;
			BakeOptions.bReplaceActors = true;
			BakeOptions.bReplaceAssets = true;
			BakeOptions.bRecenterBakedActors = CurrentHC->GetRecenterBakedActors();

			bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(CurrentHC, BakeOptions, BakeOutputs);
			FHoudiniEngineBakeUtils::SaveBakedPackages(BakeOutputs.PackagesToSave);
			
			if (bSuccess)
			{
				// Instantiate blueprints in component's level, then remove houdini asset actor
				bSuccess = false;
				ULevel* Level = CurrentHC->GetLevel();
				if (IsValid(Level))
				{
					UWorld* World = Level->GetWorld();
					if (IsValid(World))
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.OverrideLevel = Level;
						FTransform Transform = CurrentHC->GetComponentTransform();
						for (UBlueprint* Blueprint : BakeOutputs.Blueprints)
						{
							if (!IsValid(Blueprint))
								continue;
							World->SpawnActor(Blueprint->GetBlueprintClass(), &Transform, SpawnParams);
						}

						FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(CurrentHC);
						bSuccess = true;
					}
				}
			}
		}
		else
		{
			FHoudiniBakeSettings BakeOptions;
			BakeOptions.bReplaceActors = true;
			BakeOptions.bReplaceAssets = true;
			BakeOptions.bRecenterBakedActors = CurrentHC->GetRecenterBakedActors();
			if (FHoudiniEngineBakeUtils::BakeCookableToActors(CurrentHC, BakeOptions))
			{
				bSuccess = true;
				FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(CurrentHC);
			}
		}

		if (bSuccess)
			BakedCount++;
	}

	// Add a slate notification
	Notification = TEXT("Baked ") + FString::FromInt(BakedCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Baked all %d Houdini assets in the current level."), BakedCount);
}

void
FHoudiniEngineCommands::PauseAssetCooking()
{
	// Revert the global flag
	bool bCurrentCookingEnabled = !FHoudiniEngine::Get().IsCookingEnabled();
	FHoudiniEngine::Get().SetCookingEnabled(bCurrentCookingEnabled);

	// We need to refresh UI when pause cooking. Set refresh UI counter to be the number of current registered HCs.
	if (!bCurrentCookingEnabled)
		FHoudiniEngine::Get().SetUIRefreshCountWhenPauseCooking( FHoudiniEngineRuntime::Get().GetRegisteredHoudiniCookableCount() );

	// Add a slate notification
	FString Notification = TEXT("Houdini Engine cooking paused");
	if (bCurrentCookingEnabled)
		Notification = TEXT("Houdini Engine cooking resumed");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	if (bCurrentCookingEnabled)
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine cooking resumed."));
	else
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine cooking paused."));
}

bool
FHoudiniEngineCommands::IsAssetCookingPaused()
{
	return !FHoudiniEngine::Get().IsCookingEnabled();
}

void
FHoudiniEngineCommands::RecookSelection()
{
	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, true);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Cooking selected Houdini Assets...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Iterates over the selection and cook the assets if they're in a valid state
	int32 CookedCount = 0;
	for (int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++)
	{
		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Idx]);
		if (!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniCookable* HoudiniCookable = HoudiniAssetActor->GetHoudiniCookable();
		if (!IsValid(HoudiniCookable))
			continue;

		HoudiniCookable->MarkAsNeedCook();
		CookedCount++;
	}

	// Add a slate notification
	Notification = TEXT("Re-cooking ") + FString::FromInt(CookedCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Re-cooking %d selected Houdini assets."), CookedCount);
}

void
FHoudiniEngineCommands::RecookAllAssets()
{
	// Add a slate notification
	FString Notification = TEXT("Cooking all assets in the current level...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Bakes and replaces with blueprints all Houdini Assets in the current level
	int32 CookedCount = 0;
	for (TObjectIterator<UHoudiniCookable> Itr; Itr; ++Itr)
	{
		UHoudiniCookable* HoudiniCookable = *Itr;
		if (!IsValid(HoudiniCookable))
			continue;

		HoudiniCookable->MarkAsNeedCook();
		CookedCount++;
	}

	// Add a slate notification
	Notification = TEXT("Re-cooked ") + FString::FromInt(CookedCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Re-cooked %d Houdini assets in the current level."), CookedCount);
}

void
FHoudiniEngineCommands::RebuildAllAssets()
{
	// Add a slate notification
	FString Notification = TEXT("Re-building all assets in the current level...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Bakes and replaces with blueprints all Houdini Assets in the current level
	int32 RebuiltCount = 0;
	for (TObjectIterator<UHoudiniCookable> Itr; Itr; ++Itr)
	{
		UHoudiniCookable* HoudiniCookable = *Itr;
		if (!IsValid(HoudiniCookable))
			continue;

		HoudiniCookable->MarkAsNeedRebuild();
		RebuiltCount++;
	}

	// Add a slate notification
	Notification = TEXT("Rebuilt ") + FString::FromInt(RebuiltCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Rebuilt %d Houdini assets in the current level."), RebuiltCount);
}

void
FHoudiniEngineCommands::RebuildSelection()
{
	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, true);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Rebuilding selected Houdini Assets...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Iterates over the selection and rebuilds the assets if they're in a valid state
	int32 RebuiltCount = 0;
	for (int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++)
	{
		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Idx]);
		if (!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniCookable* HoudiniCookable = HoudiniAssetActor->GetHoudiniCookable();
		if (!IsValid(HoudiniCookable))
			continue;

		HoudiniCookable->MarkAsNeedRebuild();
		RebuiltCount++;
	}

	// Add a slate notification
	Notification = TEXT("Rebuilt ") + FString::FromInt(RebuiltCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Rebuilt %d selected Houdini assets."), RebuiltCount);
}

void
FHoudiniEngineCommands::BakeSelection()
{
	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, true);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Baking selected Houdini Asset Actors in the current level...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Iterates over the selection and rebuilds the assets if they're in a valid state
	int32 BakedCount = 0;
	for (int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++)
	{
		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Idx]);
		if (!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniCookable* HC = HoudiniAssetActor->GetHoudiniCookable();
		if (!IsValid(HC))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to export a Houdini Asset in the scene! - Invalid Houdini Cookable"));
			continue;
		}

		// If component is not cooking or instancing, we can bake blueprint.
		if (HC->IsInstantiatingOrCooking())
			continue;

		FHoudiniBakedObjectData BakeOutputs;
		const bool bReplaceAssets = true;

		FHoudiniBakeSettings BakeOptions;
		BakeOptions.bReplaceActors = true;
		BakeOptions.bReplaceAssets = true;
		BakeOptions.bRecenterBakedActors = HC->GetRecenterBakedActors();

		const bool bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(HC, BakeOptions, BakeOutputs);
		FHoudiniEngineBakeUtils::SaveBakedPackages(BakeOutputs.PackagesToSave);
			
		if (!bSuccess)
			continue;

		// Instantiate blueprints in component's level, then remove houdini asset actor
		ULevel* Level = HC->GetLevel();
		if (!IsValid(Level))
			continue;
		UWorld* World = Level->GetWorld();
		if (!IsValid(World))
			continue;

		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = Level;
		FTransform Transform = HC->GetComponentTransform();
		for (UBlueprint* Blueprint : BakeOutputs.Blueprints)
		{
			if (!IsValid(Blueprint))
				continue;
			World->SpawnActor(Blueprint->GetBlueprintClass(), &Transform, SpawnParams);
		}

		FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HC);
		BakedCount++;		
	}

	// Add a slate notification
	Notification = TEXT("Baked ") + FString::FromInt(BakedCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Baked all %d Houdini assets in the current level."), BakedCount);
}

// Recentre HoudiniAsset actors' pivots to their input / cooked static-mesh average centre.
void 
FHoudiniEngineCommands::RecentreSelection()
{
	// TODO: Finish me! This has been deactivated for all v2
	/*
	//Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, true);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Recentering selected Houdini Assets...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// Iterates over the selection and cook the assets if they're in a valid state
	int32 RecentreCount = 0;
	for (int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++)
	{
		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Idx]);
		if (!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniCookable * HoudiniCookable = HoudiniAssetActor->GetHoudiniCookable();
		if (!HoudiniCookable || !HoudiniCookable->IsComponentValid())
			continue;

		// Get the average centre of all the created Static Meshes
		FVector AverageBoundsCentre = FVector::ZeroVector;
		int32 NumBounds = 0;
		const FVector CurrentLocation = HoudiniCookable->GetComponentLocation();
		{
			//Check Static Meshes
			TArray<UStaticMesh*> StaticMeshes;
			StaticMeshes.Reserve(16);
			
			for (auto& CurOutput : HoudiniCookable->GetOutputs())
			{
				for (auto& CurOutputObject : CurOutput->GetOutputObjects())
				{
					CurOutputObject.Value.OutputObject
				}
			}
			HoudiniCookable->GetAllUsedStaticMeshes(StaticMeshes);

			//Get average centre of all  the static meshes.
			for (const UStaticMesh* pMesh : StaticMeshes)
			{
				if (!pMesh)
					continue;

				//to world space
				AverageBoundsCentre += (pMesh->GetBounds().Origin + CurrentLocation);
				NumBounds++;
			}
		}

		//Check Inputs
		if (0 == NumBounds)
		{
			const TArray< UHoudiniInput* >& AssetInputs = HoudiniCookable->Inputs;
			for (const UHoudiniInput* pInput : AssetInputs)
			{
				if (!IsValid(pInput))
					continue;

				// to world space
				FBox Bounds = pInput->GetInputBounds(CurrentLocation);
				if (Bounds.IsValid)
				{
					AverageBoundsCentre += Bounds.GetCenter();
					NumBounds++;
				}
			}
		}

		//if we have more than one, get the average centre
		if (NumBounds > 1)
		{
			AverageBoundsCentre /= (float)NumBounds;
		}

		//if we need to move...
		float fDist = FVector::DistSquared(CurrentLocation, AverageBoundsCentre);
		if (NumBounds && fDist > 1.0f)
		{
			// Move actor to average centre and recook
			// This will refresh the static mesh under the HoudiniAssestComponent ( undoing the translation ).
			HoudiniAssetActor->SetActorLocation(AverageBoundsCentre, false, nullptr, ETeleportType::TeleportPhysics);

			// Recook now the houdini-static-mesh has a new origin
			HoudiniCookable->StartTaskAssetCookingManual();
			RecentreCount++;
		}
	}

	if (RecentreCount)
	{
		// UE4 Editor doesn't refresh the translation-handles until they are re-selected, confusing the user, deselect the objects.
		GEditor->SelectNone(true, false);
	}

	// Add a slate notification
	Notification = TEXT("Re-centred ") + FString::FromInt(RecentreCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Re-centred %d selected Houdini assets."), RecentreCount);
	*/
}

void
FHoudiniEngineCommands::OpenSessionSync(bool bWaitForCompletion)
{
	FHoudiniEngine::Get().OpenSessionSync(bWaitForCompletion);
}

void
FHoudiniEngineCommands::CloseSessionSync()
{
	if (!FHoudiniEngine::Get().StopSession())
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to stop Session Sync - HAPI Not initialized"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Stopping Houdini Session Sync...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Stopping Houdini Session Sync."));

	// Stop Houdini Session sync if it is still running!
	FProcHandle PreviousHESS = FHoudiniEngine::Get().GetHESSProcHandle();
	if (FPlatformProcess::IsProcRunning(PreviousHESS))
	{
		FPlatformProcess::TerminateProc(PreviousHESS, true);
	}
}

void
FHoudiniEngineCommands::OpenNodeSync()
{
	//FGlobalTabmanager::Get()->TryInvokeTab(NodeSyncTabName);
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(NodeSyncTabName);
}

void FHoudiniEngineCommands::OpenHoudiniToolsTab()
{
	// FGlobalTabmanager::Get()->TryInvokeTab(HoudiniToolsTabName);
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(HoudiniToolsTabName);
}


void
FHoudiniEngineCommands::SetViewportSync(const int32& ViewportSync)
{
	if (ViewportSync == 1)
	{
		FHoudiniEngine::Get().SetSyncViewportEnabled(true);
		FHoudiniEngine::Get().SetSyncHoudiniViewportEnabled(true);
		FHoudiniEngine::Get().SetSyncUnrealViewportEnabled(false);
	}
	else if (ViewportSync == 2)
	{
		FHoudiniEngine::Get().SetSyncViewportEnabled(true);
		FHoudiniEngine::Get().SetSyncHoudiniViewportEnabled(false);
		FHoudiniEngine::Get().SetSyncUnrealViewportEnabled(true);
	}
	else if (ViewportSync == 3)
	{
		FHoudiniEngine::Get().SetSyncViewportEnabled(true);
		FHoudiniEngine::Get().SetSyncHoudiniViewportEnabled(true);
		FHoudiniEngine::Get().SetSyncUnrealViewportEnabled(true);
	}
	else
	{
		FHoudiniEngine::Get().SetSyncViewportEnabled(false);
		FHoudiniEngine::Get().SetSyncHoudiniViewportEnabled(false);
		FHoudiniEngine::Get().SetSyncUnrealViewportEnabled(false);
	}
}

int32
FHoudiniEngineCommands::GetViewportSync()
{
	if(!FHoudiniEngine::Get().IsSyncViewportEnabled())
		return 0;

	bool bSyncH = FHoudiniEngine::Get().IsSyncHoudiniViewportEnabled();
	bool bSyncU = FHoudiniEngine::Get().IsSyncUnrealViewportEnabled();
	if (bSyncH && !bSyncU)
		return 1;
	else if (!bSyncH && bSyncU)
		return 2;
	else if (bSyncH && bSyncU)
		return 3;
	else
		return 0;
}

void
FHoudiniEngineCommands::RestartSession()
{
	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().RestartSession())
		return;

	// We've successfully restarted the Houdini Engine session,
	// We now need to notify all the Cookable that they need to re instantiate 
	// themselves in the new Houdini engine session.
	FHoudiniEngineUtils::MarkAllCookablesAsNeedInstantiation();
}

void 
FHoudiniEngineCommands::CreateSession()
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().CreateSession(HoudiniRuntimeSettings->SessionType))
		return;

	// We've successfully created the Houdini Engine session,
	// We now need to notify all the Cookables that they need to re instantiate 
	// themselves in the new Houdini engine session.
	FHoudiniEngineUtils::MarkAllCookablesAsNeedInstantiation();
}

void 
FHoudiniEngineCommands::ConnectSession()
{
	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().ConnectSession(true))
		return;

	// We've successfully connected to a Houdini Engine session,
	// We now need to notify all the cookables that they need to re instantiate 
	// themselves in the new Houdini engine session.
	FHoudiniEngineUtils::MarkAllCookablesAsNeedInstantiation();
}

bool
FHoudiniEngineCommands::IsSessionValid()
{
	return FHoudiniEngine::IsInitialized();
}

bool
FHoudiniEngineCommands::IsSessionSyncProcessValid()
{
	// Only launch Houdini in Session sync if we havent started it already!
	FProcHandle PreviousHESS = FHoudiniEngine::Get().GetHESSProcHandle();
	return FPlatformProcess::IsProcRunning(PreviousHESS);
}

void
FHoudiniEngineCommands::StopSession()
{
	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().StopSession())
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - HAPI Not initialized"));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine session stopped."));
	}
}

EHoudiniProxyRefineRequestResult
FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(bool bOnlySelectedActors, bool bSilent, bool bRefineAll, bool bOnPreSaveWorld, UWorld *OnPreSaveWorld, bool bOnPreBeginPIE)
{
	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 NumSelectedHoudiniAssets = 0;
	if (bOnlySelectedActors)
	{
		NumSelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, true);
		if (NumSelectedHoudiniAssets <= 0)
		{
			HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
			return EHoudiniProxyRefineRequestResult::Invalid;
		}
	}

	// Add a slate notification
	FString Notification = TEXT("Refining Houdini proxy meshes to static meshes...");
	// FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// First find the cookables that have meshes that we must refine
	TArray<UHoudiniCookable*> CookablesToRefine;
	TArray<UHoudiniCookable*> CookablesToCook;
	// cookables that would be candidates for refinement/cooking, but have errors
	TArray<UHoudiniCookable*> SkippedCookables;

	if (bOnlySelectedActors)
	{
		for (int32 Index = 0; Index < NumSelectedHoudiniAssets; ++Index)
		{
			AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Index]);
			if (!IsValid(HoudiniAssetActor))
				continue;

			UHoudiniCookable* Cookable = HoudiniAssetActor->GetHoudiniCookable();
			if (!IsValid(Cookable))
				continue;

			// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
			// flags passed to the function.
			TriageHoudiniCookablesForProxyMeshRefinement(
				Cookable, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, 
				CookablesToRefine, CookablesToCook, SkippedCookables);
		}
	}
	else
	{
		for (TObjectIterator<UHoudiniCookable> Itr; Itr; ++Itr)
		{
			UHoudiniCookable* HoudiniCookable = *Itr;
			if (!IsValid(HoudiniCookable))
				continue;

			if (bOnPreSaveWorld && OnPreSaveWorld && OnPreSaveWorld != HoudiniCookable->GetWorld())
				continue;

			// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
			// flags passed to the function.
			TriageHoudiniCookablesForProxyMeshRefinement(HoudiniCookable, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, CookablesToRefine, CookablesToCook, SkippedCookables);
		}
	}

	return RefineTriagedHoudiniProxyMeshesToStaticMeshes(
		CookablesToRefine,
		CookablesToCook,
		SkippedCookables,
		bSilent,
		bRefineAll,
		bOnPreSaveWorld,
		OnPreSaveWorld,
		bOnPreBeginPIE
	);
}

EHoudiniProxyRefineRequestResult 
FHoudiniEngineCommands::RefineHoudiniProxyMeshActorArrayToStaticMeshes(const TArray<AHoudiniAssetActor*>& InActorsToRefine, bool bSilent)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	return FHoudiniEngineUtils::RefineHoudiniProxyMeshActorArrayToStaticMeshes(InActorsToRefine, bSilent);
}

void 
FHoudiniEngineCommands::StartPDGCommandlet()
{
	FHoudiniEngine::Get().StartPDGCommandlet();
}

void 
FHoudiniEngineCommands::StopPDGCommandlet()
{
	FHoudiniEngine::Get().StopPDGCommandlet();
}

bool
FHoudiniEngineCommands::IsPDGCommandletRunningOrConnected()
{
	return FHoudiniEngine::Get().IsPDGCommandletRunningOrConnected();
}

bool
FHoudiniEngineCommands::IsPDGCommandletEnabled()
{
	const UHoudiniRuntimeSettings* const Settings = GetDefault<UHoudiniRuntimeSettings>();
	if (IsValid(Settings))
	{
		return Settings->bPDGAsyncCommandletImportEnabled;
	}

	return false;
}

bool
FHoudiniEngineCommands::SetPDGCommandletEnabled(bool InEnabled)
{
	UHoudiniRuntimeSettings* const Settings = GetMutableDefault<UHoudiniRuntimeSettings>();
	if (IsValid(Settings))
	{
		Settings->bPDGAsyncCommandletImportEnabled = InEnabled;
		return true;
	}

	return false;
}

void
FHoudiniEngineCommands::ClearInputManager()
{
	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
	{
		HOUDINI_LOG_WARNING(TEXT("[ClearInputManager]: Manager is null!"));
		return;
	}

	Manager->Clear();
}

void
FHoudiniEngineCommands::TriageHoudiniCookablesForProxyMeshRefinement(
	UHoudiniCookable* InHC,
	bool bRefineAll,
	bool bOnPreSaveWorld,
	UWorld *OnPreSaveWorld,
	bool bOnPreBeginPIE,
	TArray<UHoudiniCookable*> &OutToRefine,
	TArray<UHoudiniCookable*> &OutToCook,
	TArray<UHoudiniCookable*> &OutSkipped)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	FHoudiniEngineUtils::TriageHoudiniCookablesForProxyMeshRefinement(InHC,
		bRefineAll,
		bOnPreSaveWorld,
		OnPreSaveWorld,
		bOnPreBeginPIE,
		OutToRefine,
		OutToCook,
		OutSkipped);
}

EHoudiniProxyRefineRequestResult
FHoudiniEngineCommands::RefineTriagedHoudiniProxyMeshesToStaticMeshes(
	const TArray<UHoudiniCookable*>& InCookablesToRefine,
	const TArray<UHoudiniCookable*>& InCookablesToCook,
	const TArray<UHoudiniCookable*>& InSkippedCookables,
	bool bInSilent,
	bool bInRefineAll,
	bool bInOnPreSaveWorld,
	UWorld* InOnPreSaveWorld,
	bool bInOnPrePIEBeginPlay)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	return FHoudiniEngineUtils::RefineTriagedHoudiniProxyMeshesToStaticMeshes(
		InCookablesToRefine,
		InCookablesToCook,
		InSkippedCookables,
		bInSilent,
		bInRefineAll,
		bInOnPreSaveWorld,
		InOnPreSaveWorld,
		bInOnPrePIEBeginPlay);
}

void
FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(
	const TArray<UHoudiniCookable*>& InCookablesToCook,
	TSharedPtr<FSlowTask, ESPMode::ThreadSafe> InTaskProgress,
	const uint32 InNumCookablesToProcess,
	bool bInOnPreSaveWorld,
	UWorld *InOnPreSaveWorld,
	const TArray<UHoudiniCookable*> &InSuccessfulCookables,
	const TArray<UHoudiniCookable*> &InFailedCookables,
	const TArray<UHoudiniCookable*> &InSkippedCookables)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	FHoudiniEngineUtils::RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(
		InCookablesToCook,
		InTaskProgress,
		InNumCookablesToProcess,
		bInOnPreSaveWorld,
		InOnPreSaveWorld,
		InSuccessfulCookables,
		InFailedCookables,
		InSkippedCookables);
}

void
FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
	const uint32 InNumTotalCookables,
	FSlowTask* const InTaskProgress,
	const bool bCancelled, 
	const bool bOnPreSaveWorld,
	UWorld* const InOnPreSaveWorld,
	const TArray<UHoudiniCookable*> &InSuccessfulCookables, 
	const TArray<UHoudiniCookable*> &InFailedCookables,
	const TArray<UHoudiniCookable*> &InSkippedCookables)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	FHoudiniEngineUtils::RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
		InNumTotalCookables,
		InTaskProgress,
		bCancelled,
		bOnPreSaveWorld,
		InOnPreSaveWorld,
		InSuccessfulCookables,
		InFailedCookables,
		InSkippedCookables);
}

void
FHoudiniEngineCommands::RefineProxyMeshesHandleOnPostSaveWorld(const TArray<UHoudiniCookable*> &InSuccessfulCookables, uint32 InSaveFlags, UWorld* InWorld, bool bInSuccess)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	FHoudiniEngineUtils::RefineProxyMeshesHandleOnPostSaveWorld(InSuccessfulCookables, InSaveFlags, InWorld, bInSuccess);
}

void
FHoudiniEngineCommands::SetAllowPlayInEditorRefinement(
	const TArray<UHoudiniCookable*>& InCookables,
	bool bEnabled)
{
	// For H21 the code for this function was moved out of the Editor module. This function is kept around for now to ease backporting.
	FHoudiniEngineUtils::SetAllowPlayInEditorRefinement(InCookables, bEnabled);
}

void
FHoudiniEngineCommands::DumpGenericAttribute(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		HOUDINI_LOG_ERROR(TEXT(" "));
		HOUDINI_LOG_ERROR(TEXT("DumpGenericAttribute takes a class name as argument! ie: DumpGenericAttribute StaticMesh"));
		HOUDINI_LOG_ERROR(TEXT(" "));
		return;
	}

	for (int32 Idx = 0; Idx < Args.Num(); Idx++)
	{
		// Get the class name
		FString ClassName = Args[Idx];

		HOUDINI_LOG_MESSAGE(TEXT("------------------------------------------------------------------------------------------------------------"));
		HOUDINI_LOG_MESSAGE(TEXT("        Dumping GenericAttribute for Class %s"), *ClassName);
		HOUDINI_LOG_MESSAGE(TEXT("------------------------------------------------------------------------------------------------------------"));

		HOUDINI_LOG_MESSAGE(TEXT(" "));
		HOUDINI_LOG_MESSAGE(TEXT("Format: "));
		HOUDINI_LOG_MESSAGE(TEXT("unreal_uproperty_XXXX : NAME (DISPLAY_NAME) - UE TYPE: UETYPE - H TYPE: HTYPE TUPLE."));
		HOUDINI_LOG_MESSAGE(TEXT(" "));
		HOUDINI_LOG_MESSAGE(TEXT(" "));

		// Make sure we can find the class
		UClass* FoundClass = FHoudiniEngineRuntimeUtils::GetClassByName(ClassName);
		if (!IsValid(FoundClass) && (ClassName.StartsWith("U") || ClassName.StartsWith("F")))
		{
			// Try again after removing the starting U/F character
			FString ChoppedName = ClassName.RightChop(1);
			FoundClass = FHoudiniEngineRuntimeUtils::GetClassByName(ChoppedName);
		}

		if (!IsValid(FoundClass))
		{
			HOUDINI_LOG_ERROR(TEXT("DumpGenericAttribute wasn't able to find a UClass that matches %s!"), *ClassName);
			HOUDINI_LOG_MESSAGE(TEXT("------------------------------------------------------------------------------------------------------------"));
			return;
		}

		UObject* ObjectToParse = FoundClass->GetDefaultObject();
		if (!IsValid(ObjectToParse))
		{
			// Use the class directly if we failed to get a DCO
			ObjectToParse = FoundClass;
		}

		// Reuse the find property function used by the generic attribute system
		FProperty* FoundProperty = nullptr;
		UObject* FoundPropertyObject = nullptr;
		void* Container = nullptr;
		FEditPropertyChain FoundPropertyChain;
		bool bExactPropertyFound = false;
		FHoudiniGenericAttribute::FindPropertyOnObject(ObjectToParse, FString(), FoundPropertyChain, FoundProperty, FoundPropertyObject, Container, bExactPropertyFound, true);

		HOUDINI_LOG_MESSAGE(TEXT("------------------------------------------------------------------------------------------------------------"));
		HOUDINI_LOG_MESSAGE(TEXT(" "));
	}
}


void
FHoudiniEngineCommands::CleanHoudiniEngineSession()
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to clean the current Houdini Engine Session - HAPI is not initialized."));
		return;
	}

	// We need a current session
	const HAPI_Session* CurrentSession = FHoudiniEngine::Get().GetSession();
	if (!CurrentSession)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to clean the current Houdini Engine Session - no current session."));
		return;
	}

	// We need the current session to be valid
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(CurrentSession))
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to clean the current Houdini Engine Session - the current session is invalid."));
		return;
	}

	HAPI_Result Result;
	HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::Cleanup(CurrentSession));
	if (HAPI_RESULT_SUCCESS != Result)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to clean up the current Houdini Engine Session."));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Succesfully cleaned up the current Houdini Engine Session."));

		// We need to reinitialize the session after the clean up
		FHoudiniEngine::Get().InitializeHAPISession();
	}
}


void
FHoudiniEngineCommands::StartPerformanceMonitoring()
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - HAPI is not initialized."));
		return;
	}

	// We need a current session
	const HAPI_Session* CurrentSession = FHoudiniEngine::Get().GetSession();
	if (!CurrentSession)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - no current session."));
		return;
	}

	// We need the current session to be valid
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(CurrentSession))
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - the current session is invalid."));
		return;
	}

	FHoudiniEngine::Get().StartHAPIPerformanceMonitoring();
}

void
FHoudiniEngineCommands::StopPerformanceMonitoring()
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - HAPI is not initialized."));
		return;
	}

	// We need a current session
	const HAPI_Session* CurrentSession = FHoudiniEngine::Get().GetSession();
	if (!CurrentSession)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - no current session."));
		return;
	}

	// We need the current session to be valid
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(CurrentSession))
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to start HAPI performance monitoring - the current session is invalid."));
		return;
	}

	FString TraceStorePath;
	if (TraceStorePath.IsEmpty())
	{
		using UE::Trace::FStoreClient;
		FStoreClient* StoreClientPtr = FStoreClient::Connect(TEXT("localhost"));
		TUniquePtr<FStoreClient> StoreClient = TUniquePtr<FStoreClient>(StoreClientPtr);

		if (StoreClient)
		{
			const FStoreClient::FStatus* Status = StoreClient->GetStatus();
			if (Status)
			{
				TraceStorePath = FString(Status->GetStoreDir());
			}
		}
	}

	FHoudiniEngine::Get().StopHAPIPerformanceMonitoring(TraceStorePath);
}

void
FHoudiniEngineCommands::DumpNode(const TArray<FString>& Args)
{
	// HAPI needs to be initialized
	if(!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("HAPI is not initialized."));
		return;
	}

	// We need a current session
	const HAPI_Session* CurrentSession = FHoudiniEngine::Get().GetSession();
	if(!CurrentSession)
	{
		HOUDINI_LOG_ERROR(TEXT("No current session."));
		return;
	}

	// We need the current session to be valid
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(CurrentSession))
	{
		HOUDINI_LOG_ERROR(TEXT("The current session is invalid."));
		return;
	}

    if(Args.Num() < 1)
    {
        HOUDINI_LOG_ERROR(TEXT("DumpNode takes a node id as argument! ie: DumpNode /obj/node"));
        return;
    }

	FHoudiniEngineUtils::DumpNode(Args[0]);
}

#undef LOCTEXT_NAMESPACE
