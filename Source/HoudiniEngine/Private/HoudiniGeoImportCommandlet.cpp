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

#include "HoudiniGeoImportCommandlet.h"

#include "DirectoryWatcherModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Guid.h"
#include "EditorFramework/AssetImportData.h"

#include "Editor.h"
#include "FileHelpers.h"

#include "MessageEndpointBuilder.h"

#include "PackageTools.h"

#include "IDirectoryWatcher.h"

#include "Internationalization/Regex.h"

#include "Interfaces/ISlateNullRendererModule.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ThreadManager.h"

#include "HoudiniPackageParams.h"
#include "HoudiniGeoImporter.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutput.h"
#include "HoudiniPDGImporterMessages.h"
#include "HoudiniEngineRuntimeUtils.h"


UHoudiniGeoImportCommandlet::UHoudiniGeoImportCommandlet()
{
	HelpDescription = TEXT("Import BGEOs as UAssets. Includes an option to watch a directories and include new .bgeos created there.");

	HelpUsage = TEXT("HoudiniGeoImport Usage: HoudiniGeoImport {options} [filename.bgeo]");
	//	"Options:\n"
	//	"\t-help or -?\n"
	//	"\t\tDisplays this help.\n\n"
	//	"\t-listen=manager_messaging_address\n\n"
	//	"\t\tListen for connections from the HoudiniEngine plug-in's PDG manager, and import bgeo files/assets requested by the PDG manager.\n\n"
	//	"\t-watch=directory\n\n"
	//	"\t\tA directory to watch for new .bgeo files to import.\n\n"
	//	"\t-managerpid=owner_pid\n\n"
	//	"\t\tThe PID of the owner/manager process. If the manager process dies the commandlet also quits.\n\n"
	//	"\t-bake\n\n"
	//	"\t\tBake generated assets. Instancers are baked to blueprints. Not supported in -listen mode.\n\n"
	//	"\t[filename.bgeo]\n"
	//	"\t\tWhen not using -listen or -watch, the path to a .bgeo file must be specified for import.\n"
	//);

	HelpParamNames = {
		"help", 
		"listen",
		"guid",
		"watch",
		"managerpid",
		"bake"
	};

	HelpParamDescriptions = {
		"Displays this help.", 
		"Listen for connections from the HoudiniEngine plug-in's PDG manager, and import bgeo files/assets requested by the PDG manager. Expects the owning process' PID.",
		"Specify a GUID for the commandlet. Useful to identify the commandlet when the messaging system is used.",
		"A directory to watch for new .bgeo files to import.",
		"The PID of the owner/manager process. If the manager process dies the commandlet also quits.",
		"Bake generated assets. Instancers are baked to blueprints. Not supported in -listen mode."
	};

	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowProgress = false;
	ShowErrorCount = false;

	// LogToConsole = false;

	Mode = EHoudiniGeoImportCommandletMode::None;
	bBakeOutputs = false;
}

void UHoudiniGeoImportCommandlet::PrintUsage() const
{
	HOUDINI_LOG_DISPLAY(TEXT("%s"), *HelpDescription);
	HOUDINI_LOG_DISPLAY(TEXT("%s"), *HelpUsage);
	const int32 NumOptions = HelpParamNames.Num();
	for (int32 Idx = 0; Idx < NumOptions; ++Idx)
	{
		HOUDINI_LOG_DISPLAY(TEXT("-%s\t%s"), *HelpParamNames[Idx], *HelpParamDescriptions[Idx]);
	}
}

void UHoudiniGeoImportCommandlet::PopulatePackageParams(const FString &InBGEOFilename, FHoudiniPackageParams& OutPackageParams)
{
	UObject* InParent = this;

	if (bBakeOutputs)
	{
		OutPackageParams.PackageMode = EPackageMode::Bake;
	}
	else
	{
		OutPackageParams.PackageMode = EPackageMode::CookToTemp;
	}
	OutPackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;

	OutPackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	OutPackageParams.BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
	
	OutPackageParams.HoudiniAssetName = FPaths::GetBaseFilename(InBGEOFilename);
	OutPackageParams.HoudiniAssetActorName = FString();
	OutPackageParams.ObjectName = FPaths::GetBaseFilename(InBGEOFilename);

	if (!OutPackageParams.OuterPackage)
	{
		OutPackageParams.OuterPackage = InParent;
	}

	if (!OutPackageParams.ComponentGUID.IsValid())
	{
		// TODO: will need to reuse the GUID when reimporting?
		OutPackageParams.ComponentGUID = FGuid::NewGuid();
	}
}

void UHoudiniGeoImportCommandlet::TickDiscoveredFiles()
{
	for (auto &FileDataEntry : DiscoveredFiles)
	{
		FDiscoveredFileData &FileData = FileDataEntry.Value;
		if (FileData.bImportNextTick && !FileData.bImported)
		{
			FileData.bImportNextTick = false;
			FileData.ImportAttempts++;
			
			FHoudiniPackageParams PackageParams;
			PopulatePackageParams(FileData.FileName, PackageParams);
			TArray<UHoudiniOutput*> Outputs;
			int32 Error = ImportBGEO(FileData.FileName, PackageParams, Outputs);
			if (Error == 0)
			{
				FileData.bImported = true;
				HOUDINI_LOG_DISPLAY(TEXT("Importing %s... Done"), *FileData.FileName);
			}
			else
			{
				FileData.bImported = false;
				HOUDINI_LOG_DISPLAY(TEXT("Importing %s... Failed (%d)"), *FileData.FileName, Error);
			}
		}
	}
}

int32 UHoudiniGeoImportCommandlet::MainLoop()
{
	GIsRunning = true;

	IDirectoryWatcher* DirectoryWatcher = nullptr;
	
	if (Mode == EHoudiniGeoImportCommandletMode::Listen)
	{
		PDGEndpoint = FMessageEndpoint::Builder("PDG/BGEO Commandlet")
			.Handling<FHoudiniPDGImportBGEOMessage>(this, &UHoudiniGeoImportCommandlet::HandleImportBGEOMessage)
			.ReceivingOnThread(ENamedThreads::GameThread);
		if (!PDGEndpoint.IsValid())
		{
			GIsRunning = false;
			return 3;
		}
		// Notify the manager that we are running
		HOUDINI_LOG_DISPLAY(TEXT("Notifying the manager (%s) that we are running"), *ManagerAddress.ToString());
		// Try to send directly to the manager
		// TODO: this initially direct message does not work, the address looks to be correct, perhaps there is some
		// additional set up needed to connect / discover the endpoints?
		PDGEndpoint->Send(new FHoudiniPDGImportBGEODiscoverMessage(Guid), ManagerAddress);
	}
	else if (Mode == EHoudiniGeoImportCommandletMode::Watch)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		DirectoryWatcher = DirectoryWatcherModule.Get();
	}

	// In UnrealEngine 4.25 and older we cannot tick the editor engine without slate being initialized.
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::InitHighDPI(false);
		FSlateApplication::Create();
	}

	// If slate is initialized, make sure it has a renderer. If we have to create a renderer, create the null renderer.
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetRenderer())
	{
		const TSharedPtr<FSlateRenderer> SlateRenderer = FModuleManager::Get().LoadModuleChecked<ISlateNullRendererModule>("SlateNullRenderer").CreateSlateNullRenderer();
		const TSharedRef<FSlateRenderer> SlateRendererSharedRef = SlateRenderer.ToSharedRef();
		FSlateApplication::Get().InitializeRenderer(SlateRendererSharedRef);
	}

	// in listen mode broadcast our presence every 60 seconds
	// This is an attempt to test if it solves a rare issue where the endpoints appear to get
	// "disconnected" and sending a message to a previously valid message address stops working, even though
	// both processes are still running (happens especially when debugging with breakpoints)
	const float BroadcastIntervalSeconds = 60.0f;
	float LastbroadcastTimeSeconds = 0.0f;
	
	// main loop
	while (GIsRunning && !IsEngineExitRequested())
	{
		GEngine->UpdateTimeAndHandleMaxTickRate();
		GEngine->Tick(FApp::GetDeltaTime(), false);

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().PumpMessages();
			FSlateApplication::Get().Tick();
		}

		// Required for FTimerManager to function - as it blocks ticks, if the frame counter doesn't change
		GFrameCounter++;

		// update task graph
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		FTSTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
		FThreadManager::Get().Tick();
		GEngine->TickDeferredCommands();

		if (DirectoryWatcherHandle.IsValid() && DirectoryWatcher)
		{
			// DirectoryWatcher->Tick(FApp::GetDeltaTime());

			// Process the discovered files
			TickDiscoveredFiles();
		}
		
		if (OwnerProcHandle.IsValid() && !FPlatformProcess::IsProcRunning(OwnerProcHandle))
		{
			// Our once valid owner has disappeared, so quit.
			RequestEngineExit(TEXT("OwnerDisappeared"));
		}

		if (Mode == EHoudiniGeoImportCommandletMode::Listen && PDGEndpoint.IsValid())
		{
			const float TimeSeconds = FPlatformTime::Seconds();
			if (TimeSeconds - LastbroadcastTimeSeconds >= BroadcastIntervalSeconds)
			{
				LastbroadcastTimeSeconds = TimeSeconds;
				// Broadcast a discover message to notify that we are still available
				PDGEndpoint->Publish(new FHoudiniPDGImportBGEODiscoverMessage(Guid));

				HOUDINI_LOG_MESSAGE(TEXT("Publishing FHoudiniPDGImportBGEODiscoverMessage(%s)"), *Guid.ToString());
			}
		}
		
		FPlatformProcess::Sleep(0);
	}

	PDGEndpoint.Reset();
	if (DirectoryWatcherHandle.IsValid() && DirectoryWatcher)
	{
		DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(DirectoryToWatch, DirectoryWatcherHandle);
	}

	if (FSlateApplication::IsInitialized())
		FSlateApplication::Shutdown();

	GIsRunning = false;

	return 0;
}

void
UHoudiniGeoImportCommandlet::HandleImportBGEOMessage(
	const FHoudiniPDGImportBGEOMessage& InMessage, 
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	HOUDINI_LOG_DISPLAY(TEXT("Received BGEO import request from %s"), *InContext->GetSender().ToString());

	FHoudiniPackageParams PackageParams;
	InMessage.PopulatePackageParams(PackageParams);

	TArray<UHoudiniOutput*> Outputs;
	TMap<FHoudiniOutputObjectIdentifier, TArray<FHoudiniGenericAttribute>> OutputObjectAttributes;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData> InstancedOutputPartData;
	if (ImportBGEO(InMessage.FilePath, PackageParams, Outputs, &InMessage.StaticMeshGenerationProperties, &InMessage.MeshBuildSettings, &OutputObjectAttributes, &InstancedOutputPartData) == 0)
	{
		FHoudiniPDGImportBGEOResultMessage* Reply = new FHoudiniPDGImportBGEOResultMessage();
		(*Reply) = InMessage;
		// Reply->PopulateFromPackageParams(PackageParams);
		Reply->ImportResult = EHoudiniPDGImportBGEOResult::HPIBR_Success;

		const int32 NumOutputs = Outputs.Num();
		Reply->Outputs.SetNumUninitialized(NumOutputs);
		for (int32 n = 0; n < Reply->Outputs.Num(); n++)
			Reply->Outputs[n] = FHoudiniPDGImportNodeOutput();

		for (int32 Index = 0; Index < NumOutputs; ++Index)
		{
			FHoudiniPDGImportNodeOutput &MessageOutput = Reply->Outputs[Index];
			UHoudiniOutput* Output = Outputs[Index];
			for (const FHoudiniGeoPartObject& HGPO : Output->GetHoudiniGeoPartObjects())
			{
				HOUDINI_LOG_WARNING(TEXT("HGPO %d %d %d"), HGPO.ObjectId, HGPO.GeoId, HGPO.PartId);
				MessageOutput.HoudiniGeoPartObjects.Add(HGPO);

				// Get instancer data if this is an instancer output
				if (Output->GetType() == EHoudiniOutputType::Instancer)
				{
					FHoudiniOutputObjectIdentifier OutputIdentifier;
					OutputIdentifier.ObjectId = HGPO.ObjectId;
					OutputIdentifier.GeoId = HGPO.GeoId;
					OutputIdentifier.PartId = HGPO.PartId;
					OutputIdentifier.PartName = HGPO.PartName;

					FHoudiniInstancedOutputPartData* InstancedOutputPartDataPtr = InstancedOutputPartData.Find(OutputIdentifier);
					if (InstancedOutputPartDataPtr)
					{
						InstancedOutputPartDataPtr->BuildFlatInstancedTransformsAndObjectPaths();
						MessageOutput.InstancedOutputPartData.Add(*InstancedOutputPartDataPtr);
					}
					else
					{
						MessageOutput.InstancedOutputPartData.Add(FHoudiniInstancedOutputPartData());
					}
				}
			}
			for (const auto& Entry : Output->GetOutputObjects())
			{
				HOUDINI_LOG_WARNING(TEXT("Identifier %d %d %d"), Entry.Key.ObjectId, Entry.Key.GeoId, Entry.Key.PartId);

				MessageOutput.OutputObjects.AddDefaulted();
				FHoudiniPDGImportNodeOutputObject& MessageOutputObject = MessageOutput.OutputObjects.Last();
				
				FString PackagePath = IsValid(Entry.Value.OutputObject) ? Entry.Value.OutputObject->GetPathName() : "";
				MessageOutputObject.Identifier = Entry.Key;
				MessageOutputObject.PackagePath = PackagePath;
				const TArray<FHoudiniGenericAttribute>* PropertyAttributes = OutputObjectAttributes.Find(Entry.Key);
				if (PropertyAttributes)
					MessageOutputObject.GenericAttributes = *PropertyAttributes;
				MessageOutputObject.CachedAttributes = Entry.Value.CachedAttributes;
			}
		}

		PDGEndpoint->Send(Reply, InContext->GetSender());
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("BGEO import failed."));
		FHoudiniPDGImportBGEOResultMessage* Reply = new FHoudiniPDGImportBGEOResultMessage();
		Reply->ImportResult = EHoudiniPDGImportBGEOResult::HPIBR_Failed;
		PDGEndpoint->Send(Reply, InContext->GetSender());
	}

	// Cleanup the outputs (remove from root)
	TArray<UPackage*> PackagesToUnload;
	for (UHoudiniOutput *CurOutput : Outputs)
	{
		if (!IsValid(CurOutput))
			continue;

		for (const auto& Entry : CurOutput->GetOutputObjects())
		{
			if (IsValid(Entry.Value.OutputObject))
			{
				UPackage *Outermost = Entry.Value.OutputObject->GetOutermost();
				if (IsValid(Outermost))
				{
					PackagesToUnload.Add(Outermost);
				}
				
				Entry.Value.OutputObject->RemoveFromRoot();
			}
		}

		CurOutput->RemoveFromRoot();
	}
	Outputs.Empty();
	OutputObjectAttributes.Empty();

	if (PackagesToUnload.Num() > 0)
	{
		HOUDINI_LOG_DISPLAY(TEXT("Unloading %d packages ..."), PackagesToUnload.Num());
		FText ErrorMessage;
		if (!UPackageTools::UnloadPackages(PackagesToUnload, ErrorMessage))
		{
			HOUDINI_LOG_WARNING(TEXT("Unload packages failed: %s"), *ErrorMessage.ToString());
		}
		else
		{
			HOUDINI_LOG_DISPLAY(TEXT("Unloading %d packages ... Success"), PackagesToUnload.Num());
		}
		PackagesToUnload.Empty();
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

bool UHoudiniGeoImportCommandlet::StartHoudiniEngineSession()
{
	// Start Houdini Engine session
	HOUDINI_LOG_DISPLAY(TEXT("Starting Houdini Engine session..."));
	FHoudiniEngine& HoudiniEngine = FHoudiniEngine::Get();
	if (!HoudiniEngine.CreateSession(
		EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe,
		"hapi_bgeo_cmdlet"))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to start Houdini Engine session."));
		return false;
	}

	return true;
}

int32 UHoudiniGeoImportCommandlet::ImportBGEO(
	const FString &InFilename, 
	const FHoudiniPackageParams &InPackageParams,
	TArray<UHoudiniOutput*>& OutOutputs,
	const FHoudiniStaticMeshGenerationProperties* InStaticMeshGenerationProperties,
	const FMeshBuildSettings* InMeshBuildSettings,
	TMap<FHoudiniOutputObjectIdentifier, TArray<FHoudiniGenericAttribute>>* OutGenericAttributes,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* OutInstancedOutputPartData)
{
	if (!IsHoudiniEngineSessionRunning() && !StartHoudiniEngineSession())
	{
		return 2;
	}

	FHoudiniPackageParams PackageParams = InPackageParams;
	UHoudiniGeoImporter* GeoImporter = NewObject<UHoudiniGeoImporter>(this);

	TArray<UHoudiniOutput*> OldOutputs;
	OutOutputs.Empty();

	// 2. Update the file paths
	HOUDINI_LOG_DISPLAY(TEXT("SetFilePath %s"), *InFilename);
	if (!GeoImporter->SetFilePath(InFilename))
		return 1;

	// 3. Load the BGEO file in HAPI
	HAPI_NodeId NodeId;
	HOUDINI_LOG_DISPLAY(TEXT("LoadBGEOFileInHAPI"));
	if (!GeoImporter->LoadBGEOFileInHAPI(NodeId))
		return 1;

	// Look for a bake folder override in the BGEO file
	if (PackageParams.PackageMode == EPackageMode::Bake)
	{
		HOUDINI_LOG_DISPLAY(TEXT("Looking for bake folder override attribute..."));
		// Get the geo id for the node id
		HAPI_GeoInfo DisplayGeoInfo;
		FHoudiniApi::GeoInfo_Init(&DisplayGeoInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), NodeId, &DisplayGeoInfo))
		{
			TArray<FString> BakeFolderOverrideArray;
			FString BakeFolderOverride;
			const bool bFoundOverride = FHoudiniEngineUtils::GetBakeFolderAttribute(
				DisplayGeoInfo.nodeId, HAPI_ATTROWNER_DETAIL, BakeFolderOverrideArray, 0, 1);

			if (bFoundOverride && BakeFolderOverrideArray.Num() > 0)
				BakeFolderOverride = BakeFolderOverrideArray[0];

			if (!BakeFolderOverride.IsEmpty())
			{
				PackageParams.BakeFolder = BakeFolderOverride;
				HOUDINI_LOG_DISPLAY(TEXT("Found bake folder override (detail attrib): %s"), *PackageParams.BakeFolder);
			}
			else
			{
				HOUDINI_LOG_DISPLAY(TEXT("No bake folder override, using: %s"), *PackageParams.BakeFolder);
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find display geo node id (when looking for bake folder override)."));
		}
	}

	auto CleanUpAndExit = [&OutOutputs, GeoImporter, NodeId](int32 InExitCode)
	{
		GeoImporter->GetOutputObjects().Empty();
		for (UHoudiniOutput* Output : OutOutputs)
		{
			Output->RemoveFromRoot();
		}
		OutOutputs.Empty();

		if (NodeId >= 0)
			GeoImporter->DeleteCreatedNode(NodeId);
		
		return InExitCode;
	};

	// 4. Get the output from the file node
	HOUDINI_LOG_DISPLAY(TEXT("BuildOutputsForNode %d"), NodeId);
	if (!GeoImporter->BuildOutputsForNode(NodeId, OldOutputs, OutOutputs))
		return CleanUpAndExit(1);

	// Create uniquely named packages, commandlet runs in conjunction
	// with a main editor instance, so we cannot modify existing files
	PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;

	// FString PackageName;
	// UPackage* Outer = PackageParams.CreatePackageForObject(PackageName);
	UObject* Outer = this;
	
	// 5. Create the static meshes in the outputs
	const FHoudiniStaticMeshGenerationProperties& StaticMeshGenerationProperties =
		InStaticMeshGenerationProperties?
		*InStaticMeshGenerationProperties :
		FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	
	const FMeshBuildSettings& MeshBuildSettings =
		InMeshBuildSettings ? *InMeshBuildSettings : FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
	
	HOUDINI_LOG_DISPLAY(TEXT("Create Static Meshes"));
	if (!GeoImporter->CreateStaticMeshes(OutOutputs, Outer, PackageParams, StaticMeshGenerationProperties, MeshBuildSettings))
		return CleanUpAndExit(1);

	//// 6. Create the landscape in the outputs
	//if (!GeoImporter->CreateLandscapes(NewOutputs, Outer, PackageParams))
	//	return CleanUpAndExit(1);

	// 7. Create the instancers in the outputs
	if (OutInstancedOutputPartData)
	{
		if (!GeoImporter->CreateInstancerOutputPartData(OutOutputs, *OutInstancedOutputPartData))
			return CleanUpAndExit(1);
	}
	else
	{
		if (!GeoImporter->CreateInstancers(OutOutputs, Outer, PackageParams))
			return CleanUpAndExit(1);
	}

	if (OutGenericAttributes)
	{
		// Collect all generic properties from Houdini, we need to pass these 
		// through to PDG manager
		HOUDINI_LOG_DISPLAY(TEXT("Get Generic Attributes for static meshes"));
		for (UHoudiniOutput* CurOutput : OutOutputs)
		{
			if (CurOutput->GetType() != EHoudiniOutputType::Mesh)
				continue;
			
			for (const auto& Entry : CurOutput->GetOutputObjects())
			{
				const FHoudiniOutputObjectIdentifier OutputIdentifier = Entry.Key;
				TArray<FHoudiniGenericAttribute> PropertyAttributes;
				FHoudiniEngineUtils::GetGenericPropertiesAttributes(
                    OutputIdentifier.GeoId, OutputIdentifier.PartId,
                    true, OutputIdentifier.PrimitiveIndex, INDEX_NONE, OutputIdentifier.PointIndex,
                    PropertyAttributes);
				OutGenericAttributes->Add(OutputIdentifier, PropertyAttributes);
			}
		}
	}

	// 8. Delete the created  node in Houdini
	HOUDINI_LOG_DISPLAY(TEXT("DeleteCreatedNode %d"), NodeId);
	if (!GeoImporter->DeleteCreatedNode(NodeId))
	{
		// Not good, but not fatal..
		//return false;
	}

	TArray<UPackage*> PackagesToSave;
	TArray<UObject*>& OutputObjects = GeoImporter->GetOutputObjects();
	for (UObject* Object : OutputObjects)
	{
		if (!IsValid(Object))
			continue;

		HOUDINI_LOG_DISPLAY(TEXT("Created object: %s"), *Object->GetFullName());

		UAssetImportData* AssetImportData = nullptr;
		if (Object->IsA<UStaticMesh>())
		{
			UStaticMesh* SM = Cast<UStaticMesh>(Object);
			AssetImportData = SM->AssetImportData;
			// Create reimport information.
			if (!AssetImportData)
			{
				AssetImportData = NewObject< UAssetImportData >(SM, UAssetImportData::StaticClass());
				SM->AssetImportData = AssetImportData;
			}
		}
		
		if (AssetImportData)
			AssetImportData->Update(InFilename);
		
		Object->MarkPackageDirty();
		Object->PostEditChange();

		UPackage* Package = Object->GetOutermost();
		if (IsValid(Package))
		{
			PackagesToSave.AddUnique(Package);
		}
	}

	if (PackagesToSave.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
	}
	
	PackagesToSave.Empty();
	OutputObjects.Empty();

	return 0;
}

void UHoudiniGeoImportCommandlet::HandleDirectoryChanged(const TArray<FFileChangeData>& InFileChangeDatas)
{
	const FRegexPattern BGEOPattern(TEXT(R"((.*)\.(bgeo(\.[^\.]*)?)$)"));
	
	for (const FFileChangeData& FileChangeData : InFileChangeDatas)
	{
		HOUDINI_LOG_MESSAGE(TEXT("HandleDirectoryChanged %d %s"), FileChangeData.Action, *FileChangeData.Filename);

		FRegexMatcher BGEOMatcher(BGEOPattern, FileChangeData.Filename.ToLower());
		if (BGEOMatcher.FindNext() && BGEOMatcher.GetCaptureGroup(2).StartsWith(TEXT("bgeo")))
		{
			HOUDINI_LOG_DISPLAY(TEXT("Updating entry for %s..."), *FileChangeData.Filename);
			const uint32 MaxImportAttempts = 3;
			switch(FileChangeData.Action)
			{
				case FFileChangeData::FCA_Added:
				case FFileChangeData::FCA_Modified:
					if (DiscoveredFiles.Contains(FileChangeData.Filename))
					{
						FDiscoveredFileData &FileData = DiscoveredFiles[FileChangeData.Filename];
						if (!FileData.bImported && FileData.ImportAttempts < MaxImportAttempts)
							FileData.bImportNextTick = true;
						else if (FileData.ImportAttempts >= MaxImportAttempts)
							HOUDINI_LOG_WARNING(TEXT("Not importing %s, max attempts exceeded %d"), *FileData.FileName, FileData.ImportAttempts);
					}
					else
					{
						DiscoveredFiles.Add(FileChangeData.Filename, FDiscoveredFileData(FileChangeData.Filename, true));
					}
				break;
				case FFileChangeData::FCA_Removed:
					DiscoveredFiles.Remove(FileChangeData.Filename);
				break;
				default:
					HOUDINI_LOG_WARNING(TEXT("Unknown file change event %d for %s"), FileChangeData.Action, *FileChangeData.Filename);
			}
		}
	}
}

int32 UHoudiniGeoImportCommandlet::Main(const FString& InParams)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*InParams, Tokens, Switches, Params);

	if (Switches.Contains(TEXT("help")) || Switches.Contains(TEXT("?")))
	{
		PrintUsage();
		return 0;
	}

	if (Params.Contains(TEXT("guid")))
	{
		const FString GuidStr = Params.FindChecked(TEXT("guid"));
		FGuid::Parse(GuidStr, Guid);
		
		HOUDINI_LOG_DISPLAY(TEXT("GUID received: %s"), *Guid.ToString());
	}
	else
	{
		Guid = FGuid::NewGuid();
	}

	// Set bake mode
	if (Switches.Contains(TEXT("bake")))
		bBakeOutputs = true;
	else
		bBakeOutputs = false;		
	
	if (Params.Contains(TEXT("listen")))
	{
		Mode = EHoudiniGeoImportCommandletMode::Listen;
		
		if (!Params.Contains(TEXT("managerpid")))
		{
			HOUDINI_LOG_ERROR(TEXT("'managerpid' is required when in -listen mode."));
			return 1;
		}

		if (bBakeOutputs)
		{
			HOUDINI_LOG_ERROR(TEXT("'listen' mode does not support baking outputs (-bake)."));
			return 1;
		}

		// Get the manager's messaging address from the -listen param
		const FString ManagerAddressStr = Params.FindChecked(TEXT("listen"));
		if (!FMessageAddress::Parse(ManagerAddressStr, ManagerAddress))
		{
			HOUDINI_LOG_ERROR(TEXT("The manager messaging address passed to -listen=%s is invalid."), *ManagerAddressStr);
			return 1;
		}
		
		// Get the manager pid and proc handle
		uint32 OwnerProcessId = FCString::Strtoi(*Params.FindChecked(TEXT("managerpid")), nullptr, 10);
		HOUDINI_LOG_DISPLAY(TEXT("Owner process Id: %d"), OwnerProcessId);
		OwnerProcHandle = FPlatformProcess::OpenProcess(OwnerProcessId);
		
		return MainLoop();
	}
	else if (Params.Contains(TEXT("watch")))
	{
		Mode = EHoudiniGeoImportCommandletMode::Watch;
		
		HOUDINI_LOG_DISPLAY(TEXT("directory watch mode"));
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			DirectoryToWatch = Params.FindChecked(TEXT("watch"));
			if (FPaths::IsRelative(DirectoryToWatch))
				DirectoryToWatch = FPaths::ConvertRelativePathToFull(DirectoryToWatch);

			HOUDINI_LOG_DISPLAY(TEXT("Watching %s"), *DirectoryToWatch);

			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				DirectoryToWatch,
				IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UHoudiniGeoImportCommandlet::HandleDirectoryChanged),
				DirectoryWatcherHandle);
			
			return MainLoop();
		}
		else
		{
			return 10;
		}
	}
	else if (Tokens.Num() > 0)
	{
		Mode = EHoudiniGeoImportCommandletMode::SpecifiedFiles;
		
		if (!StartHoudiniEngineSession())
			return 2;

		const FString Filename = FPaths::IsRelative(Tokens[0]) ? FPaths::ConvertRelativePathToFull(Tokens[0]) : Tokens[0];
		FHoudiniPackageParams PackageParams;
		PopulatePackageParams(Filename, PackageParams);

		TArray<UHoudiniOutput*> Outputs;
		const int32 Result = ImportBGEO(Tokens[0], PackageParams, Outputs);

		for (UHoudiniOutput* Output : Outputs)
		{
			Output->RemoveFromRoot();
		}
		Outputs.Empty();

		return Result;
	}

	return 0;
}
