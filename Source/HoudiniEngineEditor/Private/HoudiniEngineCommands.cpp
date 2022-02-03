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
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniOutput.h"
#include "HoudiniEngineStyle.h"

#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "EditorDirectories.h"
#include "Misc/ScopedSlowTask.h"
#include "Async/Async.h"
#include "FileHelpers.h"
#include "AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"
#include "ObjectTools.h"
#include "CoreGlobals.h"
#include "HoudiniEngineOutputStats.h"
#include "Misc/FeedbackContext.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "UObject/ObjectSaveContext.h"
//#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

FDelegateHandle FHoudiniEngineCommands::OnPostSaveWorldRefineProxyMeshesHandle = FDelegateHandle();
FHoudiniEngineCommands::FOnHoudiniProxyMeshesRefinedDelegate FHoudiniEngineCommands::OnHoudiniProxyMeshesRefinedDelegate = FHoudiniEngineCommands::FOnHoudiniProxyMeshesRefinedDelegate();

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
	UI_COMMAND(_OpenSessionSync, "Open Houdini Session Sync", "Opens Houdini with Session Sync and connect to it.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_CloseSessionSync, "Close Houdini Session Sync", "Close the Session Sync Houdini.", EUserInterfaceActionType::Button, FInputChord());

	// Viewport Sync
	UI_COMMAND(_ViewportSyncNone, "Disabled", "Do not sync viewports.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncUnreal, "Sync Unreal to Houdini.", "Sync the Unreal viewport to Houdini's.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncHoudini, "Sync Houdini to Unreal", "Sync the Houdini viewport to Unreal's.", EUserInterfaceActionType::Check, FInputChord());
	UI_COMMAND(_ViewportSyncBoth, "Both", "Sync both Unreal and Houdini's viewport.", EUserInterfaceActionType::Check, FInputChord());

	// PDG Import Commandlet
	UI_COMMAND(_StartPDGCommandlet, "Start Async Importer", "Start the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_StopPDGCommandlet, "Stop Async Importer", "Stops the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_IsPDGCommandletEnabled, "Enable Async Importer", "Enables the commandlet that imports PDG BGEO results in the background.", EUserInterfaceActionType::Check, FInputChord());
	
	UI_COMMAND(_InstallInfo, "Installation Info", "Display information on the current Houdini Engine installation", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_PluginSettings, "PluginSettings", "Displays the Houdini Engine plugin settings", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(_OpenInHoudini, "Open scene in Houdini", "Opens the current Houdini scene in Houdini.", EUserInterfaceActionType::Button, FInputChord(EKeys::O, EModifierKey::Control | EModifierKey::Alt));
	UI_COMMAND(_SaveHIPFile, "Save Houdini scene (HIP)", "Saves a .hip file of the current Houdini scene.", EUserInterfaceActionType::Button, FInputChord());
		
	UI_COMMAND(_OnlineDoc, "Online Documentation", "Go to the plugin's online documentation.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_OnlineForum, "Online Forum", "Go to the plugin's online forum.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(_ReportBug, "Report a bug", "Report a bug for Houdini Engine for Unreal plugin.", EUserInterfaceActionType::Button, FInputChord());

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

	TArray< FString > SaveFilenames;
	bool bSaved = false;
	void * ParentWindowWindowHandle = NULL;

	IMainFrameModule & MainFrameModule = FModuleManager::LoadModuleChecked< IMainFrameModule >(TEXT("MainFrame"));
	const TSharedPtr< SWindow > & MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();

	bSaved = DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		NSLOCTEXT("SaveHIPFile", "SaveHIPFile", "Saves a .hip file of the current Houdini scene.").ToString(),
		*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
		TEXT(""),
		TEXT("Houdini HIP file|*.hip"),
		EFileDialogFlags::None,
		SaveFilenames);

	if (bSaved && SaveFilenames.Num())
	{
		// Add a slate notification
		FString Notification = TEXT("Saving internal Houdini scene...");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		// ... and a log message
		HOUDINI_LOG_MESSAGE(TEXT("Saved Houdini scene to %s"), *SaveFilenames[0]);

		// Get first path.
		std::string HIPPathConverted(TCHAR_TO_UTF8(*SaveFilenames[0]));

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

	// First, saves the current scene as a hip file
	// Creates a proper temporary file name
	FString UserTempPath = FPaths::CreateTempFilename(
		FPlatformProcess::UserTempDir(),
		TEXT("HoudiniEngine"), TEXT(".hip"));

	// Save HIP file through Engine.
	std::string TempPathConverted(TCHAR_TO_UTF8(*UserTempPath));
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

	// Then open the hip file in Houdini
	FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
	FString HoudiniExecutable = FHoudiniEngine::Get().GetHoudiniExecutable();
	FString HoudiniLocation = LibHAPILocation + TEXT("//") + HoudiniExecutable;

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*HoudiniLocation,
		*UserTempPath,
		true, false, false,
		nullptr, 0,
		FPlatformProcess::UserTempDir(),
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
			FPlatformProcess::UserTempDir(),
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
	// TODO
}

void
FHoudiniEngineCommands::ShowPluginSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("HoudiniEngine"));
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
	for (TObjectIterator<UHoudiniAssetComponent> It; It; ++It)
	{
		FString CookFolder = It->TemporaryCookFolder.Path;
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
				bool bReferencedInMemoryOrUndoStack = IsReferenced(AssetInPackage, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &ReferencesIncludingUndo);
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
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
		if (!IsValid(HoudiniAssetComponent))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to bake a Houdini Asset in the scene! - Invalid Houdini Asset Component"));
			continue;
		}

		if (!HoudiniAssetComponent->IsComponentValid())
		{
			FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
			if (AssetName != "Default__HoudiniAssetActor")
				HOUDINI_LOG_ERROR(TEXT("Failed to bake a Houdini Asset in the scene! -  %s is invalid"), *AssetName);
			continue;
		}

		// If component is not cooking or instancing, we can bake blueprint.
		if (HoudiniAssetComponent->IsInstantiatingOrCooking())
		{
			FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
			HOUDINI_LOG_ERROR(TEXT("Failed to bake a Houdini Asset in the scene! -  %s is actively instantiating or cooking"), *AssetName);
			continue;
		}

		bool bSuccess = false;
		bool BakeToBlueprints = true;
		if (BakeToBlueprints)
		{
			// if (FHoudiniEngineBakeUtils::ReplaceWithBlueprint(HoudiniAssetComponent) != nullptr)
			// 	bSuccess = true;
			FHoudiniEngineOutputStats BakeStats;
			TArray<UPackage*> PackagesToSave;
			TArray<UBlueprint*> Blueprints;
			const bool bInReplaceAssets = true;
			bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(HoudiniAssetComponent, bInReplaceAssets, HoudiniAssetComponent->bRecenterBakedActors, BakeStats, Blueprints, PackagesToSave);
			FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);
			
			if (bSuccess)
			{
				// Instantiate blueprints in component's level, then remove houdini asset actor
				bSuccess = false;
				ULevel* Level = HoudiniAssetComponent->GetComponentLevel();
				if (IsValid(Level))
				{
					UWorld* World = Level->GetWorld();
					if (IsValid(World))
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.OverrideLevel = Level;
						FTransform Transform = HoudiniAssetComponent->GetComponentTransform();
						for (UBlueprint* Blueprint : Blueprints)
						{
							if (!IsValid(Blueprint))
								continue;
							World->SpawnActor(Blueprint->GetBlueprintClass(), &Transform, SpawnParams);
						}

						FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);
						bSuccess = true;
					}
				}
			}
		}
		else
		{
			// TODO: this used to have a way to not select in v1
			// if (FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithActors(HoudiniAssetComponent))
			// 	bSuccess = true;
			const bool bReplaceActors = true;
			const bool bReplaceAssets = true;
			if (FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(HoudiniAssetComponent, bReplaceActors, bReplaceAssets, HoudiniAssetComponent->bRecenterBakedActors))
			{
				bSuccess = true;
				FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);
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

	// We need to refresh UI when pause cooking. Set refresh UI counter to be the number of current registered HACs.
	if (!bCurrentCookingEnabled)
		FHoudiniEngine::Get().SetUIRefreshCountWhenPauseCooking( FHoudiniEngineRuntime::Get().GetRegisteredHoudiniComponentCount() );

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

	if (!bCurrentCookingEnabled)
		return;

	/*
	// If we are unpausing, tick each asset component to "update" them
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
		if (!IsValid(HoudiniAssetComponent) || !HoudiniAssetComponent->IsValidLowLevel())
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to cook a Houdini Asset in the scene!"));
			continue;
		}

		HoudiniAssetComponent->StartHoudiniTicking();
	}
	*/
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

		UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!IsValid(HoudiniAssetComponent))
			continue;

		HoudiniAssetComponent->MarkAsNeedCook();
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
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
		if (!IsValid(HoudiniAssetComponent))
			continue;

		HoudiniAssetComponent->MarkAsNeedCook();
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
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
		if (!IsValid(HoudiniAssetComponent))
			continue;

		HoudiniAssetComponent->MarkAsNeedRebuild();
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

		UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!IsValid(HoudiniAssetComponent))// || !HoudiniAssetComponent->IsComponentValid())
			continue;

		HoudiniAssetComponent->MarkAsNeedRebuild();
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

		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!IsValid(HoudiniAssetComponent))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to export a Houdini Asset in the scene!"));
			continue;
		}

		if (!HoudiniAssetComponent->IsComponentValid())
		{
			FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
			HOUDINI_LOG_ERROR(TEXT("Failed to export Houdini Asset: %s in the scene!"), *AssetName);
			continue;
		}

		// If component is not cooking or instancing, we can bake blueprint.
		if (!HoudiniAssetComponent->IsInstantiatingOrCooking())
		{
			// if (FHoudiniEngineBakeUtils::ReplaceWithBlueprint(HoudiniAssetComponent) != nullptr)
			// 	BakedCount++;
			// if (FHoudiniEngineBakeUtils::ReplaceWithBlueprint(HoudiniAssetComponent) != nullptr)
			// 	bSuccess = true;
			FHoudiniEngineOutputStats BakeStats;
			TArray<UPackage*> PackagesToSave;
			TArray<UBlueprint*> Blueprints;
			const bool bReplaceAssets = true;
			const bool bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(HoudiniAssetComponent, bReplaceAssets, HoudiniAssetComponent->bRecenterBakedActors, BakeStats, Blueprints, PackagesToSave);
			FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);
			
			if (bSuccess)
			{
				// Instantiate blueprints in component's level, then remove houdini asset actor
				ULevel* Level = HoudiniAssetComponent->GetComponentLevel();
				if (IsValid(Level))
				{
					UWorld* World = Level->GetWorld();
					if (IsValid(World))
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.OverrideLevel = Level;
						FTransform Transform = HoudiniAssetComponent->GetComponentTransform();
						for (UBlueprint* Blueprint : Blueprints)
						{
							if (!IsValid(Blueprint))
								continue;
							World->SpawnActor(Blueprint->GetBlueprintClass(), &Transform, SpawnParams);
						}

						FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);
						BakedCount++;
					}
				}
			}
		}
	}

	// Add a slate notification
	Notification = TEXT("Baked ") + FString::FromInt(BakedCount) + TEXT(" Houdini assets.");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Baked all %d Houdini assets in the current level."), BakedCount);
}

// Recentre HoudiniAsset actors' pivots to their input / cooked static-mesh average centre.
void FHoudiniEngineCommands::RecentreSelection()
{
	/*
#if WITH_EDITOR
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

		UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!HoudiniAssetComponent || !HoudiniAssetComponent->IsComponentValid())
			continue;

		// Get the average centre of all the created Static Meshes
		FVector AverageBoundsCentre = FVector::ZeroVector;
		int32 NumBounds = 0;
		const FVector CurrentLocation = HoudiniAssetComponent->GetComponentLocation();
		{
			//Check Static Meshes
			TArray<UStaticMesh*> StaticMeshes;
			StaticMeshes.Reserve(16);
			HoudiniAssetComponent->GetAllUsedStaticMeshes(StaticMeshes);

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
			const TArray< UHoudiniInput* >& AssetInputs = HoudiniAssetComponent->Inputs;
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
			HoudiniAssetComponent->StartTaskAssetCookingManual();
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

#endif //WITH_EDITOR
	*/
}

void
FHoudiniEngineCommands::OpenSessionSync()
{
	//if (!FHoudiniEngine::IsInitialized())
	//	return;

	if (!FHoudiniEngine::Get().StopSession())
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to start Session Sync - HAPI Not initialized"));
		return;
	}

	// Get the runtime settings to get the session/type and settings
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	EHoudiniRuntimeSettingsSessionType SessionType = HoudiniRuntimeSettings->SessionType;
	FString ServerPipeName = HoudiniRuntimeSettings->ServerPipeName;
	int32 ServerPort = HoudiniRuntimeSettings->ServerPort;
	
	FString SessionSyncArgs = TEXT("-hess=");
	if (SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe)
	{
		// Add the -hess=pipe:hapi argument
		SessionSyncArgs += TEXT("pipe:") + ServerPipeName;
	}
	else if (SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_Socket)
	{
		// Add the -hess=port:9090 argument
		SessionSyncArgs += TEXT("port:") + FString::FromInt(ServerPort);
	}
	else
	{
		// Invalid session type
		HOUDINI_LOG_ERROR(TEXT("Failed to start Session Sync - Invalid session type"));
		return;
	}

	// Add a slate notification
	FString Notification = TEXT("Opening Houdini Session Sync...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Opening Houdini Session Sync."));

	// Only launch Houdini in Session sync if we havent started it already!
	FProcHandle PreviousHESS = FHoudiniEngine::Get().GetHESSProcHandle();
	if (!FPlatformProcess::IsProcRunning(PreviousHESS))
	{
		// Start houdini with the -hess commandline args
		const FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
#		if PLATFORM_MAC
			const FString HoudiniExeLocationRelativeToLibHAPI = TEXT("/../Resources/bin");
#		elif PLATFORM_LINUX
			const FString HoudiniExeLocationRelativeToLibHAPI = TEXT("/../bin");
#		elif PLATFORM_WINDOWS
			const FString HoudiniExeLocationRelativeToLibHAPI;
#		else
			// Treat an unknown platform the same as Windows for now
			const FString HoudiniExeLocationRelativeToLibHAPI;
#		endif

		FString HoudiniExecutable = FHoudiniEngine::Get().GetHoudiniExecutable();
		FString HoudiniLocation = LibHAPILocation + HoudiniExeLocationRelativeToLibHAPI + TEXT("/") + HoudiniExecutable;
		HOUDINI_LOG_MESSAGE(TEXT("Path to houdini executable: %s"), *HoudiniLocation);
		FProcHandle HESSHandle = FPlatformProcess::CreateProc(
			*HoudiniLocation,
			*SessionSyncArgs,
			true, false, false,
			nullptr, 0,
			FPlatformProcess::UserTempDir(),
			nullptr, nullptr);

		if (!HESSHandle.IsValid())
		{
			// Try with the steam version executable instead
			HoudiniLocation = LibHAPILocation + HoudiniExeLocationRelativeToLibHAPI + TEXT("/hindie.steam"); 
			HOUDINI_LOG_MESSAGE(TEXT("Path to hindie.steam executable: %s"), *HoudiniLocation);

			HESSHandle = FPlatformProcess::CreateProc(
				*HoudiniLocation,
				*SessionSyncArgs,
				true, false, false,
				nullptr, 0,
				FPlatformProcess::UserTempDir(),
				nullptr, nullptr);

			if (!HESSHandle.IsValid())
			{
				HOUDINI_LOG_ERROR(TEXT("Failed to launch Houdini in Session Sync mode."));
				return;
			}
		}

		// Keep track of the SessionSync ProcHandle
		FHoudiniEngine::Get().SetHESSProcHandle(HESSHandle);
	}

	// Start an Async task to connect to Session Sync	
	Async(EAsyncExecution::TaskGraphMainThread, [SessionType, ServerPipeName, ServerPort]()
	{
		// Use a timeout to avoid waiting indefinitely for H to start in session sync mode
		const double Timeout = 180.0; // 3min
		const double StartTimestamp = FPlatformTime::Seconds();

		FString ServerHost = TEXT("localhost");
		while (!FHoudiniEngine::Get().SessionSyncConnect(SessionType, ServerPipeName, ServerHost, ServerPort))
		{
			// Houdini might not be done loading, sleep for one second 
			FPlatformProcess::Sleep(1);

                        // Check for license error
                        int32 HESSReturnCode;
                        FProcHandle HESSHandle = FHoudiniEngine::Get().GetHESSProcHandle();
                        if (FPlatformProcess::GetProcReturnCode(HESSHandle, &HESSReturnCode))
                        {
                            FString Notification = TEXT("Failed to start SessionSync...");
                            FHoudiniEngineUtils::CreateSlateNotification(Notification);

                            switch (HESSReturnCode)
                            {
                                case 3:
                                    HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - No licenses were available"));
                                    FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::NoLicense);
                                    return false;
                                    break;
                                default:
                                    HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - Unknown error"));
                                    FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::Failed);
                                    return false;
                                    break;
                            }
                        }

			// Check for the timeout
			if (FPlatformTime::Seconds() - StartTimestamp > Timeout)
			{
				// ... and a log message
				HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - Timeout..."));
				return false;
			}
		}

		// Initialize HAPI with this session
		if (!FHoudiniEngine::Get().InitializeHAPISession())
		{
			FHoudiniEngine::Get().StopTicking();
			return false;
		}
		
		// Notify all HACs that they need to instantiate in the new session
		MarkAllHACsAsNeedInstantiation();
		
		// Start ticking
		FHoudiniEngine::Get().StartTicking();

		// Add a slate notification
		FString Notification = TEXT("Succesfully connected to Session Sync...");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		// ... and a log message
		HOUDINI_LOG_MESSAGE(TEXT("Succesfully connected to Session Sync..."));
		
		return true;
	});
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
	// We now need to notify all the HoudiniAssetComponent that they need to re instantiate 
	// themselves in the new Houdini engine session.
	MarkAllHACsAsNeedInstantiation();
}

void 
FHoudiniEngineCommands::CreateSession()
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().CreateSession(HoudiniRuntimeSettings->SessionType))
		return;

	// We've successfully created the Houdini Engine session,
	// We now need to notify all the HoudiniAssetComponent that they need to re instantiate 
	// themselves in the new Houdini engine session.
	MarkAllHACsAsNeedInstantiation();
}

void 
FHoudiniEngineCommands::ConnectSession()
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Restart the current Houdini Engine Session
	if (!FHoudiniEngine::Get().ConnectSession(HoudiniRuntimeSettings->SessionType))
		return;

	// We've successfully connected to a Houdini Engine session,
	// We now need to notify all the HoudiniAssetComponent that they need to re instantiate 
	// themselves in the new Houdini engine session.
	MarkAllHACsAsNeedInstantiation();
}

void
FHoudiniEngineCommands::MarkAllHACsAsNeedInstantiation()
{	
	// Notify all the HoudiniAssetComponents that they need to re instantiate themselves in the new Houdini engine session.
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
		if (!IsValid(HoudiniAssetComponent))
			continue;

		HoudiniAssetComponent->MarkAsNeedInstantiation();
	}
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

	// First find the components that have meshes that we must refine
	TArray<UHoudiniAssetComponent*> ComponentsToRefine;
	TArray<UHoudiniAssetComponent*> ComponentsToCook;
	// Components that would be candidates for refinement/cooking, but have errors
	TArray<UHoudiniAssetComponent*> SkippedComponents;
	if (bOnlySelectedActors)
	{
		for (int32 Index = 0; Index < NumSelectedHoudiniAssets; ++Index)
		{
			AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Index]);
			if (!IsValid(HoudiniAssetActor))
				continue;

			UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
			if (!IsValid(HoudiniAssetComponent))
				continue;

			// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
			// flags passed to the function.
			TriageHoudiniAssetComponentsForProxyMeshRefinement(HoudiniAssetComponent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, ComponentsToRefine, ComponentsToCook, SkippedComponents);
		}
	}
	else
	{
		for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
		{
			UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
			if (!IsValid(HoudiniAssetComponent))
				continue;

			if (bOnPreSaveWorld && OnPreSaveWorld && OnPreSaveWorld != HoudiniAssetComponent->GetWorld())
				continue;

			// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
			// flags passed to the function.
			TriageHoudiniAssetComponentsForProxyMeshRefinement(HoudiniAssetComponent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, ComponentsToRefine, ComponentsToCook, SkippedComponents);
		}
	}

	return RefineTriagedHoudiniProxyMesehesToStaticMeshes(
		ComponentsToRefine,
		ComponentsToCook,
		SkippedComponents,
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
	const bool bRefineAll = true;
	const bool bOnPreSaveWorld = false;
	UWorld* OnPreSaveWorld = nullptr;
	const bool bOnPreBeginPIE = false;

	// First find the components that have meshes that we must refine
	TArray<UHoudiniAssetComponent*> ComponentsToRefine;
	TArray<UHoudiniAssetComponent*> ComponentsToCook;
	// Components that would be candidates for refinement/cooking, but have errors
	TArray<UHoudiniAssetComponent*> SkippedComponents;
	for (const AHoudiniAssetActor* HoudiniAssetActor : InActorsToRefine)
	{
		if (!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!IsValid(HoudiniAssetComponent))
			continue;

		// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
		// flags passed to the function.
		TriageHoudiniAssetComponentsForProxyMeshRefinement(HoudiniAssetComponent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, ComponentsToRefine, ComponentsToCook, SkippedComponents);
	}

	return RefineTriagedHoudiniProxyMesehesToStaticMeshes(
		ComponentsToRefine,
		ComponentsToCook,
		SkippedComponents,
		bSilent,
		bRefineAll,
		bOnPreSaveWorld,
		OnPreSaveWorld,
		bOnPreBeginPIE
	);
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
FHoudiniEngineCommands::TriageHoudiniAssetComponentsForProxyMeshRefinement(UHoudiniAssetComponent* InHAC, bool bRefineAll, bool bOnPreSaveWorld, UWorld *OnPreSaveWorld, bool bOnPreBeginPIE, TArray<UHoudiniAssetComponent*> &OutToRefine, TArray<UHoudiniAssetComponent*> &OutToCook, TArray<UHoudiniAssetComponent*> &OutSkipped)
{
	if (!IsValid(InHAC))
		return;

	// Make sure that the component's World and Owner are valid
	AActor *Owner = InHAC->GetOwner();
	if (!IsValid(Owner))
		return;

	UWorld *World = InHAC->GetWorld();
	if (!IsValid(World))
		return;

	if (bOnPreSaveWorld && OnPreSaveWorld && OnPreSaveWorld != World)
		return;
	
	// Check if we should consider this component for proxy mesh refinement based on its settings and
	// flags passed to the function
	if (bRefineAll ||
		(bOnPreSaveWorld && InHAC->IsProxyStaticMeshRefinementOnPreSaveWorldEnabled()) ||
		(bOnPreBeginPIE && InHAC->IsProxyStaticMeshRefinementOnPreBeginPIEEnabled()))
	{
		TArray<UPackage*> ProxyMeshPackagesToSave;
		TArray<UHoudiniAssetComponent*> ComponentsWithProxiesToSave;
		
		if (InHAC->HasAnyCurrentProxyOutput())
		{
			// Get the state of the asset and check if it is cooked
			// If it is not cook, request a cook. We can only build the UStaticMesh
			// if the data from the cook is available
			// If the state is not pre-cook, or None (cooked), then the state is invalid,
			// log an error and skip the component
			bool bNeedsRebuildOrDelete = false;
			bool bUnsupportedState = false;
			const bool bCookedDataAvailable = InHAC->IsHoudiniCookedDataAvailable(bNeedsRebuildOrDelete, bUnsupportedState);
			if (bCookedDataAvailable)
			{
				OutToRefine.Add(InHAC);
				ComponentsWithProxiesToSave.Add(InHAC);
			}
			else if (!bUnsupportedState && !bNeedsRebuildOrDelete)
			{
				InHAC->MarkAsNeedCook();
				// Force the output of the cook to be directly created as a UStaticMesh and not a proxy
				InHAC->SetNoProxyMeshNextCookRequested(true);
				OutToCook.Add(InHAC);
				ComponentsWithProxiesToSave.Add(InHAC);
			}
			else
			{
				OutSkipped.Add(InHAC);
				const EHoudiniAssetState AssetState = InHAC->GetAssetState();
				HOUDINI_LOG_ERROR(TEXT("Could not refine %s, the asset is in an unsupported state: %s"), *(InHAC->GetPathName()), *(UEnum::GetValueAsString(AssetState)));
			}
		}
		else if (InHAC->HasAnyProxyOutput())
		{
			// If the HAC has non-current proxies, destroy them
			// TODO: Make this its own command?
			const uint32 NumOutputs = InHAC->GetNumOutputs();
			for (uint32 Index = 0; Index < NumOutputs; ++Index)
			{
				UHoudiniOutput *Output = InHAC->GetOutputAt(Index);
				if (!IsValid(Output))
					continue;

				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				for (auto& CurrentPair : OutputObjects)
				{
					FHoudiniOutputObject& CurrentOutputObject = CurrentPair.Value;
					if (!CurrentOutputObject.bProxyIsCurrent)
					{
						// The proxy is not current, delete it and its component
						USceneComponent* FoundProxyComponent = Cast<USceneComponent>(CurrentOutputObject.ProxyComponent);
						if (IsValid(FoundProxyComponent))
						{
							// Remove from the HoudiniAssetActor
							if (FoundProxyComponent->GetOwner())
								FoundProxyComponent->GetOwner()->RemoveOwnedComponent(FoundProxyComponent);

							FoundProxyComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
							FoundProxyComponent->UnregisterComponent();
							FoundProxyComponent->DestroyComponent();
						}

						UObject* ProxyObject = CurrentOutputObject.ProxyObject;
						if (!IsValid(ProxyObject))
							continue;

						ProxyObject->MarkAsGarbage();
						ProxyObject->MarkPackageDirty();
						UPackage* const Package = ProxyObject->GetPackage();
						if (IsValid(Package))
							ProxyMeshPackagesToSave.Add(Package);
					}
				}
			}
		}

		for (UHoudiniAssetComponent* const HAC : ComponentsWithProxiesToSave)
		{
			const uint32 NumOutputs = HAC->GetNumOutputs();
			for (uint32 Index = 0; Index < NumOutputs; ++Index)
			{
				UHoudiniOutput *Output = HAC->GetOutputAt(Index);
				if (!IsValid(Output))
					continue;

				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				for (auto& CurrentPair : OutputObjects)
				{
					FHoudiniOutputObject& CurrentOutputObject = CurrentPair.Value;
					if (CurrentOutputObject.bProxyIsCurrent && CurrentOutputObject.ProxyObject)
					{
						UPackage* const Package = CurrentOutputObject.ProxyObject->GetPackage();
						if (IsValid(Package) && Package->IsDirty())
							ProxyMeshPackagesToSave.Add(Package);
					}
				}
			}
		}

		if (ProxyMeshPackagesToSave.Num() > 0)
		{
			TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			FEditorFileUtils::PromptForCheckoutAndSave(ProxyMeshPackagesToSave, true, false);
		}
	}
}

EHoudiniProxyRefineRequestResult
FHoudiniEngineCommands::RefineTriagedHoudiniProxyMesehesToStaticMeshes(
	const TArray<UHoudiniAssetComponent*>& InComponentsToRefine, 
	const TArray<UHoudiniAssetComponent*>& InComponentsToCook, 
	const TArray<UHoudiniAssetComponent*>& InSkippedComponents,
	bool bInSilent,
	bool bInRefineAll,
	bool bInOnPreSaveWorld,
	UWorld* InOnPreSaveWorld,
	bool bInOnPrePIEBeginPlay)
{
	// Slate notification text
	FString Notification = TEXT("Refining Houdini proxy meshes to static meshes...");

	const uint32 NumComponentsToCook = InComponentsToCook.Num();
	const uint32 NumComponentsToRefine = InComponentsToRefine.Num();
	const uint32 NumComponentsToProcess = NumComponentsToCook + NumComponentsToRefine;
	
	TArray<UHoudiniAssetComponent*> SuccessfulComponents;
	TArray<UHoudiniAssetComponent*> FailedComponents;
	TArray<UHoudiniAssetComponent*> SkippedComponents(InSkippedComponents);

	auto AllowPlayInEditorRefinementFn = [&bInOnPrePIEBeginPlay, &InComponentsToCook, &InComponentsToRefine] (bool bEnabled, bool bRefinementDone){
		if (bInOnPrePIEBeginPlay)
		{
			// Flag the components that need cooking / refinement as cookable in PIE mode. No other cooking will be allowed.
			// Once refinement is done, we'll unset these flags again.
			SetAllowPlayInEditorRefinement(InComponentsToCook, true);
			SetAllowPlayInEditorRefinement(InComponentsToRefine, true);
			if (bRefinementDone)
			{
				// Don't tick during PIE. We'll resume ticking when PIE is stopped.
				FHoudiniEngine::Get().StopTicking();
			}
		}
	};

	AllowPlayInEditorRefinementFn(true, false);
	
	if (NumComponentsToProcess > 0)
	{
		// The task progress pointer is potentially going to be shared with a background thread and tasks
		// on the main thread, so make it thread safe
		TSharedPtr<FSlowTask, ESPMode::ThreadSafe> TaskProgress = MakeShareable(new FSlowTask((float)NumComponentsToProcess, FText::FromString(Notification)));
		TaskProgress->Initialize();
		if (!bInSilent)
			TaskProgress->MakeDialog(/*bShowCancelButton=*/true);

		// Iterate over the components for which we can build UStaticMesh, and build the meshes
		bool bCancelled = false;
		for (uint32 ComponentIndex = 0; ComponentIndex < NumComponentsToRefine; ++ComponentIndex)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = InComponentsToRefine[ComponentIndex];
			TaskProgress->EnterProgressFrame(1.0f);
			const bool bDestroyProxies = true;
			FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(HoudiniAssetComponent, bDestroyProxies);

			SuccessfulComponents.Add(HoudiniAssetComponent);

			bCancelled = TaskProgress->ShouldCancel();
			if (bCancelled)
			{
				for (uint32 SkippedIndex = ComponentIndex + 1; SkippedIndex < NumComponentsToRefine; ++SkippedIndex)
				{
					SkippedComponents.Add(InComponentsToRefine[ComponentIndex]);
				}
				break;
			}
		}

		if (bCancelled && NumComponentsToCook > 0)
		{
			for (UHoudiniAssetComponent* const HAC : InComponentsToCook)
			{
				SkippedComponents.Add(HAC);
			}
		}
		
		if (NumComponentsToCook > 0 && !bCancelled)
		{
			// Now use an async task to check on the progress of the cooking components
			Async(EAsyncExecution::Thread, [InComponentsToCook, TaskProgress, NumComponentsToProcess, bInOnPreSaveWorld, InOnPreSaveWorld, SuccessfulComponents, FailedComponents, SkippedComponents]() {
				RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(
					InComponentsToCook, TaskProgress, NumComponentsToProcess, bInOnPreSaveWorld, InOnPreSaveWorld, SuccessfulComponents, FailedComponents, SkippedComponents);
			});

			// We have to wait for cook(s) before completing refinement
			return EHoudiniProxyRefineRequestResult::PendingCooks;
		}
		else
		{
			RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
				NumComponentsToProcess, TaskProgress.Get(), bCancelled, bInOnPreSaveWorld, InOnPreSaveWorld, SuccessfulComponents, FailedComponents, SkippedComponents);

			// We didn't have to cook anything, so refinement is complete.
			AllowPlayInEditorRefinementFn(false, true);
			return EHoudiniProxyRefineRequestResult::Refined; 
		}
	}

	// Nothing to refine
	AllowPlayInEditorRefinementFn(false, true);
	return EHoudiniProxyRefineRequestResult::None; 
}


void
FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(const TArray<UHoudiniAssetComponent*>& InComponentsToCook, TSharedPtr<FSlowTask, ESPMode::ThreadSafe> InTaskProgress, const uint32 InNumComponentsToProcess, bool bInOnPreSaveWorld, UWorld *InOnPreSaveWorld, const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, const TArray<UHoudiniAssetComponent*> &InFailedComponents, const TArray<UHoudiniAssetComponent*> &InSkippedComponents)
{
	// Copy to a double linked list so that we can loop through
	// to check progress of each component and remove it easily
	// if it has completed/failed
	TDoubleLinkedList<UHoudiniAssetComponent*> CookList;
	for (UHoudiniAssetComponent *HAC : InComponentsToCook)
	{
		CookList.AddTail(HAC);
	}

	// Add the successfully cooked components to the incoming successful components (previously refined)
	TArray<UHoudiniAssetComponent*> SuccessfulComponents(InSuccessfulComponents);
	TArray<UHoudiniAssetComponent*> FailedComponents(InFailedComponents);
	TArray<UHoudiniAssetComponent*> SkippedComponents(InSkippedComponents);

	bool bCancelled = false;
	uint32 NumFailedToCook = 0;
	while (CookList.Num() > 0 && !bCancelled)
	{
		TDoubleLinkedList<UHoudiniAssetComponent*>::TDoubleLinkedListNode *Node = CookList.GetHead();
		while (Node && !bCancelled)
		{
			TDoubleLinkedList<UHoudiniAssetComponent*>::TDoubleLinkedListNode *Next = Node->GetNextNode();
			UHoudiniAssetComponent* HAC = Node->GetValue();

			if (IsValid(HAC))
			{
				const EHoudiniAssetState State = HAC->GetAssetState();
				const EHoudiniAssetStateResult ResultState = HAC->GetAssetStateResult();
				bool bUpdateProgress = false;
				if (State == EHoudiniAssetState::None)
				{
					// Cooked, count as success, remove node
					CookList.RemoveNode(Node);
					SuccessfulComponents.Add(HAC);
					bUpdateProgress = true;
				}
				else if (ResultState != EHoudiniAssetStateResult::None && ResultState != EHoudiniAssetStateResult::Working)
				{
					// Failed, remove node
					HOUDINI_LOG_ERROR(TEXT("Failed to cook %s to obtain static mesh."), *(HAC->GetPathName()));
					CookList.RemoveNode(Node);
					FailedComponents.Add(HAC);
					bUpdateProgress = true;
					NumFailedToCook++;
				}

				if (bUpdateProgress && InTaskProgress.IsValid())
				{
					// Update progress only on the main thread, and check for cancellation request
					bCancelled = Async(EAsyncExecution::TaskGraphMainThread, [InTaskProgress]() {
						InTaskProgress->EnterProgressFrame(1.0f);
						return InTaskProgress->ShouldCancel();
					}).Get();
				}
			}
			else
			{
				SkippedComponents.Add(HAC);
				CookList.RemoveNode(Node);
			}

			Node = Next;
		}
		FPlatformProcess::Sleep(0.01f);
	}

	if (bCancelled)
	{
		HOUDINI_LOG_WARNING(TEXT("Mesh refinement cancelled while waiting for %d components to cook."), CookList.Num());
		// Mark any remaining HACs in the cook list as skipped
		TDoubleLinkedList<UHoudiniAssetComponent*>::TDoubleLinkedListNode* Node = CookList.GetHead();
		while (Node)
		{
			TDoubleLinkedList<UHoudiniAssetComponent*>::TDoubleLinkedListNode* const Next = Node->GetNextNode();
			UHoudiniAssetComponent* HAC = Node->GetValue();
			if (HAC)
				SkippedComponents.Add(HAC);
			CookList.RemoveNode(Node);
			Node = Next;
		}
	}

	// Cooking is done, or failed, display the notifications on the main thread
	Async(EAsyncExecution::TaskGraphMainThread, [InNumComponentsToProcess, InTaskProgress, bCancelled, bInOnPreSaveWorld, InOnPreSaveWorld, SuccessfulComponents, FailedComponents, SkippedComponents]() {
		RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(InNumComponentsToProcess, InTaskProgress.Get(), bCancelled, bInOnPreSaveWorld, InOnPreSaveWorld, SuccessfulComponents, FailedComponents, SkippedComponents);
	});
}

void
FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(const uint32 InNumTotalComponents, FSlowTask* const InTaskProgress, const bool bCancelled, const bool bOnPreSaveWorld, UWorld* const InOnPreSaveWorld, const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, const TArray<UHoudiniAssetComponent*> &InFailedComponents, const TArray<UHoudiniAssetComponent*> &InSkippedComponents)
{
	FString Notification;
	const uint32 NumSkippedComponents = InSkippedComponents.Num();
	const uint32 NumFailedToCook = InFailedComponents.Num();
	if (NumSkippedComponents + NumFailedToCook > 0)
	{
		if (bCancelled)
		{
			Notification = FString::Printf(TEXT("Refinement cancelled after completing %d / %d components. The remaining components were skipped, in an invalid state, or could not be cooked. See the log for details."), NumSkippedComponents + NumFailedToCook, InNumTotalComponents);
		}
		else
		{
			Notification = FString::Printf(TEXT("Failed to refine %d / %d components, the components were in an invalid state, and were either not cooked or could not be cooked. See the log for details."), NumSkippedComponents + NumFailedToCook, InNumTotalComponents);
		}
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
		HOUDINI_LOG_ERROR(TEXT("%s"), *Notification);
	}
	else if (InNumTotalComponents > 0)
	{
		Notification = TEXT("Done: Refining Houdini proxy meshes to static meshes.");
		// FHoudiniEngineUtils::CreateSlateNotification(Notification);
		HOUDINI_LOG_MESSAGE(TEXT("%s"), *Notification);
	}
	if (InTaskProgress)
	{
		InTaskProgress->Destroy();
	}
	if (bOnPreSaveWorld && InSuccessfulComponents.Num() > 0)
	{
		FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineCommands::GetOnPostSaveWorldRefineProxyMeshesHandle();
		if (OnPostSaveWorldHandle.IsValid())
		{
			if (FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
				OnPostSaveWorldHandle.Reset();
		}

		// Save the dirty static meshes in InSuccessfulComponents OnPostSaveWorld
		// TODO: Remove? This may not be necessary now as we save all dirty temporary cook data in PostSaveWorldWithContext() already (Static Meshes, Materials...)
		OnPostSaveWorldHandle = FEditorDelegates::PostSaveWorldWithContext.AddLambda(
		[InSuccessfulComponents, bOnPreSaveWorld, InOnPreSaveWorld](UWorld* InWorld, FObjectPostSaveContext InContext)
		{
			if (bOnPreSaveWorld && InOnPreSaveWorld && InOnPreSaveWorld != InWorld)
				return;

			RefineProxyMeshesHandleOnPostSaveWorld(InSuccessfulComponents, InContext.GetSaveFlags(), InWorld, InContext.SaveSucceeded());

			FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineCommands::GetOnPostSaveWorldRefineProxyMeshesHandle();
			if (OnPostSaveWorldHandle.IsValid())
			{
				if (FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
					OnPostSaveWorldHandle.Reset();
			}
		});
	}

	SetAllowPlayInEditorRefinement(InSuccessfulComponents, false);
	SetAllowPlayInEditorRefinement(InFailedComponents, false);
	SetAllowPlayInEditorRefinement(InSkippedComponents, false);

	// Broadcast refinement result per HAC
	for (UHoudiniAssetComponent* const HAC : InSuccessfulComponents)
	{
		if (OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HAC, EHoudiniProxyRefineResult::Success);
	}
	for (UHoudiniAssetComponent* const HAC : InFailedComponents)
	{
		if (OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HAC, EHoudiniProxyRefineResult::Failed);
	}
	for (UHoudiniAssetComponent* const HAC : InSkippedComponents)
	{
		if (OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HAC, EHoudiniProxyRefineResult::Skipped);
	}
}

void
FHoudiniEngineCommands::RefineProxyMeshesHandleOnPostSaveWorld(const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, uint32 InSaveFlags, UWorld* InWorld, bool bInSuccess)
{
	TArray<UPackage*> PackagesToSave;

	for (UHoudiniAssetComponent* HAC : InSuccessfulComponents)
	{
		if (!IsValid(HAC))
			continue;

		const int32 NumOutputs = HAC->GetNumOutputs();
		for (int32 Index = 0; Index < NumOutputs; ++Index)
		{
			UHoudiniOutput *Output = HAC->GetOutputAt(Index);
			if (!IsValid(Output))
				continue;

			if (Output->GetType() != EHoudiniOutputType::Mesh)
				continue;

			for (auto &OutputObjectPair : Output->GetOutputObjects())
			{
				UObject *Obj = OutputObjectPair.Value.OutputObject;
				if (!IsValid(Obj))
					continue;

				UStaticMesh *SM = Cast<UStaticMesh>(Obj);
				if (!SM)
					continue;

				UPackage *Package = SM->GetOutermost();
				if (!IsValid(Package))
					continue;

				if (Package->IsDirty() && Package->IsFullyLoaded() && Package != GetTransientPackage())
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
}

void
FHoudiniEngineCommands::SetAllowPlayInEditorRefinement(
	const TArray<UHoudiniAssetComponent*>& InComponents,
	bool bEnabled)
{
#if WITH_EDITORONLY_DATA
	for (UHoudiniAssetComponent* Component : InComponents)
	{
		Component->SetAllowPlayInEditorRefinement(false);
	}
#endif
}

#undef LOCTEXT_NAMESPACE
