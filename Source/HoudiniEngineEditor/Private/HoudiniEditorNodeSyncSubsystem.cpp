// Fill out your copyright notice in the Description page of Project Settings.


#include "HoudiniEditorNodeSyncSubsystem.h"

#include "HoudiniAssetActor.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoImporter.h"
#include "HoudiniInput.h"
#include "HoudiniInputObject.h"
#include "HoudiniInputTranslator.h"
#include "HoudiniInputTypes.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniSkeletalMeshTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "UnrealMeshTranslator.h"
#include "UnrealSkeletalMeshTranslator.h"

#include "ActorFactories/ActorFactory.h"
#include "Engine/SkeletalMesh.h"
#include "LevelEditor.h"


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif

void 
UHoudiniEditorNodeSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnRegisterLayoutExtensions().AddUObject(this, &UHoudiniEditorNodeSyncSubsystem::RegisterLayoutExtensions);
}

void 
UHoudiniEditorNodeSyncSubsystem::Deinitialize()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnRegisterLayoutExtensions().RemoveAll(this);
}

void
UHoudiniEditorNodeSyncSubsystem::RegisterLayoutExtensions(FLayoutExtender& Extender)
{
	//Extender.ExtendLayout(LevelEditorTabIds::PlacementBrowser, ELayoutExtensionPosition::After, FTabManager::FTab(HoudiniNodeSyncTabId, ETabState::ClosedTab));
	Extender.ExtendLayout(_GetPlacementBrowserName(), ELayoutExtensionPosition::After, FTabManager::FTab(NodeSyncTabName, ETabState::ClosedTab));
}

bool 
UHoudiniEditorNodeSyncSubsystem::CreateSessionIfNeeded()
{
	// TODO: Improve me!
	// Attempt to restart the session
	if (!FHoudiniEngine::Get().IsSessionSyncEnabled())
	{
		FHoudiniEngine::Get().RestartSession();
	}

	// Make sure we have a valid session
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(FHoudiniEngine::Get().GetSession()))
		return false;

	// returns true if we have a session sync-enabled session
	return FHoudiniEngine::Get().IsSessionSyncEnabled();
}


bool
UHoudiniEditorNodeSyncSubsystem::GetNodeSyncInput(UHoudiniInput*& OutInput)
{
	if (!InitNodeSyncInputIfNeeded())
		return false;

	if (!IsValid(NodeSyncInput))
		return false;

	OutInput = NodeSyncInput;

	return true;
}


bool
UHoudiniEditorNodeSyncSubsystem::InitNodeSyncInputIfNeeded()
{
	if (IsValid(NodeSyncInput))
		return true;

	// Create a fake HoudiniInput/HoudiniInputObject so we can use the input Translator to send the data to H
	FString InputObjectName = TEXT("NodeSyncInput");
	NodeSyncInput = NewObject<UHoudiniInput>(
		this, UHoudiniInput::StaticClass(), FName(*InputObjectName), RF_Transactional);

	if (!IsValid(NodeSyncInput))
		return false;

	// Set the default input options
	// TODO: Fill those from the NodeSync UI?!
	NodeSyncInput->SetExportColliders(false);
	NodeSyncInput->SetExportLODs(false);
	NodeSyncInput->SetExportSockets(false);
	NodeSyncInput->SetLandscapeExportType(EHoudiniLandscapeExportType::Heightfield);
	NodeSyncInput->SetAddRotAndScaleAttributes(false);
	NodeSyncInput->SetImportAsReference(false);
	NodeSyncInput->SetImportAsReferenceRotScaleEnabled(false);
	NodeSyncInput->SetKeepWorldTransform(true);
	NodeSyncInput->SetPackBeforeMerge(false);
	NodeSyncInput->SetUnrealSplineResolution(50.0f);
	NodeSyncInput->SetExportLevelInstanceContent(true);

	// default input options
	NodeSyncInput->SetCanDeleteHoudiniNodes(false);
	NodeSyncInput->SetUseLegacyInputCurve(true);

	// TODO: Check?
	NodeSyncInput->SetAssetNodeId(-1);
	//NodeSyncInput->SetInputNodeId(UnrealFetchNodeIdId);

	// Input type? switch to world if actors in the selection ?
	bool bOutBPModif = false;
	NodeSyncInput->SetInputType(EHoudiniInputType::Geometry, bOutBPModif);
	NodeSyncInput->SetName(TEXT("NodeSyncInput"));

	return true;
}


void
UHoudiniEditorNodeSyncSubsystem::SendWorldSelection()
{
	UHoudiniEditorNodeSyncSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
	if (!IsValid(HoudiniSubsystem))
		return;

	HoudiniSubsystem->LastSendStatus = EHoudiniNodeSyncStatus::Running;
	HoudiniSubsystem->SendStatusMessage = "Sending...";

	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, false);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Node Sync: No selection in the world outliner"));

		HoudiniSubsystem->LastSendStatus = EHoudiniNodeSyncStatus::Failed;
		HoudiniSubsystem->SendStatusMessage = "Send Failed: No selection in the world outliner.";
		HoudiniSubsystem->SendStatusDetails = "Houdini Node Sync - Send Failed: No selection in the world outliner\nPlease select Actors in the World and try again.";

		return;
	}

	// Input type? switch to world when sending from world? (necessary?)
	//bool bOutBPModif = false;
	//HoudiniSubsystem->NodeSyncInput->SetInputType(EHoudiniInputType::World, bOutBPModif);

	HoudiniSubsystem->SendToHoudini(WorldSelection);
}


void 
UHoudiniEditorNodeSyncSubsystem::SendToHoudini(const TArray<UObject*>& SelectedAssets)
{
	if (SelectedAssets.Num() <= 0)
		return;

	// Add a slate notification
	FString Notification = TEXT("Sending selected assets to Houdini...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Sending To Houdini!"));

	// Update status
	LastSendStatus = EHoudiniNodeSyncStatus::Running;
	SendStatusMessage = "Sending...";

	if (!CreateSessionIfNeeded())
	{
		// For now, just warn the session is not session sync
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: the current session is not session-sync one!"));

		LastSendStatus = EHoudiniNodeSyncStatus::Warning;
		SendStatusMessage = "Warning: the current session is not Session-sync one!";
		SendStatusDetails = SendStatusMessage + "\nYou can start a Session-sync session by using the Open Session Sync entry in the Houdini Engine menu.";
	}

	// Create the content node
	// As as subnet, so it's able to contain multiple geos
	FString SendNodePath = NodeSyncOptions.SendNodePath;
	HAPI_NodeId  UnrealContentNodeId = -1;
	HAPI_Result result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*SendNodePath), &UnrealContentNodeId);
	if ((result != HAPI_RESULT_SUCCESS) || (UnrealContentNodeId < 0))
	{
		FString Name = SendNodePath;
		Name.RemoveFromStart("/obj/");
		result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, "Object/subnet", TCHAR_TO_ANSI(*Name), true, &UnrealContentNodeId);
	}

	// Get the NodeSync input from the Editor Subsystem
	UHoudiniInput* NSInput;
	if (!GetNodeSyncInput(NSInput))
		return;

	// Default input options
	NSInput->SetCanDeleteHoudiniNodes(false);
	NSInput->SetUseLegacyInputCurve(true);
		
	// TODO: Check?
	NSInput->SetAssetNodeId(-1);
	NSInput->SetInputNodeId(UnrealContentNodeId);

	const FHoudiniInputObjectSettings InputSettings(NSInput);

	// For each selected Asset, create a HoudiniInputObject and send it to H
	for (int32 Idx = 0; Idx < SelectedAssets.Num(); Idx++)
	{
		TArray<int32> CreatedNodeIds;
		TSet<FUnrealObjectInputHandle> Handles;
		UObject* CurrentObject = SelectedAssets[Idx];
		if (!IsValid(CurrentObject))
			continue;

		// Create an input object wrapper for the current object
		UHoudiniInputObject * CurrentInputObject = UHoudiniInputObject::CreateTypedInputObject(CurrentObject, NSInput, FString::FromInt(Idx + 1), InputSettings);
		if (!IsValid(CurrentInputObject))
			continue;

		// Create a geo node for this object in the content node
		FString ObjectName = CurrentObject->GetName();
		FHoudiniEngineUtils::SanitizeHAPIVariableName(ObjectName);

		// If the object is an Actor, prefer its label over the object name
		AActor* CurrentActor = Cast<AActor>(CurrentObject);
		if (IsValid(CurrentActor))
		{
			ObjectName = CurrentActor->GetActorNameOrLabel();
		}

		HAPI_NodeId CurrentObjectNodeId = -1;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CreateNode(
			FHoudiniEngine::Get().GetSession(), UnrealContentNodeId, "geo", TCHAR_TO_ANSI(*ObjectName), true, &CurrentObjectNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to create input object geo node for %s."), *CurrentInputObject->GetName());

			LastSendStatus = EHoudiniNodeSyncStatus::SuccessWithErrors;
			SendStatusMessage = "Send Success with errors";
			SendStatusDetails = "Houdini Node Sync - Send success with errors - Not all selected objects were created.";
		}

		// Preset the existing Object Node ID to the unreal content node
		// !! Do not preset, as the input system will destroy those previous input nodes!
		CurrentInputObject->SetInputNodeId(-1);
		CurrentInputObject->SetInputObjectNodeId(-1);
			
		// TODO: Transform for actors?
		FTransform CurrentActorTransform = FTransform::Identity;

		// Send the HoudiniInputObject to H
		if (!FHoudiniInputTranslator::UploadHoudiniInputObject(
			NSInput, CurrentInputObject, CurrentActorTransform, CreatedNodeIds, Handles, false))
		{
			HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to send %s to %s."), *CurrentInputObject->GetName(), *SendNodePath);

			LastSendStatus = EHoudiniNodeSyncStatus::SuccessWithErrors;
			SendStatusMessage = "Send Success with errors";
			SendStatusDetails = "Houdini Node Sync - Send success with errors - Not all selected objects were created.";

			continue;
		}

		// We've created the input nodes for this objet, now, we need to object merge them into the content node in the path specified by the user
		bool bObjMergeSuccess = false;
		for (int32 CreatedNodeIdx = 0; CreatedNodeIdx < CreatedNodeIds.Num(); CreatedNodeIdx++)
		{
			// todo: if we've created more than one node, merge them together?
			if (CreatedNodeIds.Num() > 1)
				ObjectName += TEXT("_") + FString::FromInt(CreatedNodeIdx + 1);

			HAPI_NodeId ObjectMergeNodeId = -1;
			HAPI_NodeId GeoObjectMergeNodeId = -1;
			bObjMergeSuccess &= FHoudiniInputTranslator::HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
				CurrentObjectNodeId, CreatedNodeIds[CreatedNodeIdx], ObjectName, ObjectMergeNodeId, CurrentObjectNodeId, true, FTransform::Identity, 1);
		}
	}
	

	// Update status
	if (LastSendStatus != EHoudiniNodeSyncStatus::SuccessWithErrors)
	{
		LastSendStatus = EHoudiniNodeSyncStatus::Success;
		SendStatusMessage = "Send Success";
		SendStatusDetails = "Houdini Node Sync - Send success";
	}	

	// TODO: Improve me! handle failures!
	Notification = TEXT("Houdini Node Sync success!");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
}


void 
UHoudiniEditorNodeSyncSubsystem::Fetch()
{
	LastFetchStatus = EHoudiniNodeSyncStatus::Running;
	FetchStatusMessage = "Fetching...";

	FetchFromHoudini(NodeSyncOptions.UnrealAssetName, NodeSyncOptions.UnrealAssetFolder);
}


void 
UHoudiniEditorNodeSyncSubsystem::FetchFromHoudini(
	const FString& InPackageName, const FString& InPackageFolder, const int32& InMaxInfluences, const bool& InImportNormals)
{
	// Add a slate notification
	FString Notification = TEXT("Fetching data from Houdini...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
	
	LastFetchStatus = EHoudiniNodeSyncStatus::Running;
	FetchStatusMessage = "Fetching...";

 	if (!CreateSessionIfNeeded())
	{
		// For now, just warn the session is not session sync
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: the current session is not session-sync one!"));

		LastFetchStatus = EHoudiniNodeSyncStatus::Warning;
		FetchStatusMessage = "warning: the current session is not Session-sync one!";
		FetchStatusDetails = FetchStatusMessage + "\nYou can start a Session-sync session by using the Open Session Sync entry in the Houdini Engine menu.";
	}

	// Make sure that the FetchNodePath is a valid Houdini node path pointing to a valid Node
	HAPI_NodeId  UnrealFetchNodeId = -1;
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	// Make sure we're not trying to fetch /obj, as this seems to crash HE
	FString FetchNodePath = NodeSyncOptions.FetchNodePath;
	if (FetchNodePath.Equals("/obj", ESearchCase::IgnoreCase)
		|| FetchNodePath.Equals("/obj/", ESearchCase::IgnoreCase))
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FetchNodePath"));
		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Failed: Invalid Fetch node path!";
		FetchStatusDetails = "Houdini Node Sync - Fetch Failed - Unable to fetch /obj/.";

		return;
	}


	Result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*FetchNodePath), &UnrealFetchNodeId);
	if ((Result != HAPI_RESULT_SUCCESS) || (UnrealFetchNodeId < 0))
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FetchNodePath"));
		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Failed: Invalid Fetch node path!";
		FetchStatusDetails = "Houdini Node Sync - Fetch Failed - The Fetch node path is invalid.";

		return;
	}

	// We need to gather all the required nodes
	TArray<HAPI_NodeId> FetchNodeIds;
	if (!GatherAllFetchNodeIds(UnrealFetchNodeId, NodeSyncOptions.bUseOutputNodes, FetchNodeIds))
	{
		HOUDINI_LOG_ERROR(TEXT("Houdini Node Sync: Failed to gather fetch nodes."));
		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Failed: Unable to gather fetch node outputs!";
		FetchStatusDetails = "Houdini Node Sync - Fetch Failed - Unable to gather fetch node outputs.";

		return;
	}

	// Make sure that the output nodes have been cooked
	// This ensure that we'll be able to get the proper number of parts for them
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	for (auto CurrentNodeId : FetchNodeIds)
	{
		if (!FHoudiniEngineUtils::HapiCookNode(UnrealFetchNodeId, &CookOptions, true))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to cook NodeSyncFetch node!"));
			// Only log? still try to continue with the output processing?
			// return;
		}
	}

	// We use the BGEO importer when Fetching to the contentbrowser
	bool bUseBGEOImport = !NodeSyncOptions.bFetchToWorld;
	bool bSuccess = false;
	if (bUseBGEOImport)
	{
		// Parent obj node that will contain all the merge nodes used for the import
		// This will make cleaning up the fetch node easier
		TArray<HAPI_NodeId> CreatedNodeIds;

		// Create a new Geo importer
		TArray<UHoudiniOutput*> DummyOldOutputs;
		TArray<UHoudiniOutput*> NewOutputs;
		UHoudiniGeoImporter* HoudiniGeoImporter = NewObject<UHoudiniGeoImporter>(this);
		HoudiniGeoImporter->AddToRoot();

		// Clean up lambda
		auto CleanUp = [&NewOutputs, &HoudiniGeoImporter, &CreatedNodeIds]()
		{
			// Remove the importer and output objects from the root set
			HoudiniGeoImporter->RemoveFromRoot();
			for (auto Out : NewOutputs)
				Out->RemoveFromRoot();

			// Delete the nodes created for the import
			for(auto CurrentNodeId : CreatedNodeIds)
			{
				// Delete the parent node of the created nodes
				FHoudiniEngineUtils::DeleteHoudiniNode(
					FHoudiniEngineUtils::HapiGetParentNodeId(CurrentNodeId)
				);
			}
		};
		
		// Failure lambda
		auto FailImportAndReturn = [this, &CleanUp, &NewOutputs, &HoudiniGeoImporter]()
		{
			CleanUp();

			this->LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
			this->FetchStatusMessage = "Failed";
			this->FetchStatusDetails = "Houdini Node Sync - Fetch Failed.";

			// TODO: Improve me!
			FString Notification = TEXT("Houdini Node Sync - Fetch failed!");
			FHoudiniEngineUtils::CreateSlateNotification(Notification);

			return;
		};

		// Process each fetch node with the GeoImporter
		for (auto CurrentFetchId : FetchNodeIds)
		{
			FString CurrentFetchPath;
			if (!FHoudiniEngineUtils::HapiGetAbsNodePath(CurrentFetchId, CurrentFetchPath))
				continue;

			// Create the fetch(object merge) node for the geo importer
			HAPI_NodeId CurrentFetchNodeId = -1;
			if (!HoudiniGeoImporter->MergeGeoFromNode(CurrentFetchPath, CurrentFetchNodeId))
				return FailImportAndReturn();

			// 4. Get the output from the Fetch node
			//TArray<UHoudiniOutput*> CurrentOutputs;
			if (!HoudiniGeoImporter->BuildOutputsForNode(CurrentFetchNodeId, DummyOldOutputs, NewOutputs, NodeSyncOptions.bUseOutputNodes))
				return FailImportAndReturn();

			// Keep track of the created merge node so we can delete it later on
			CreatedNodeIds.Add(CurrentFetchNodeId);
		}

		// Prepare the package used for creating the mesh, landscape and instancer pacakges
		FHoudiniPackageParams PackageParams;
		PackageParams.PackageMode = EPackageMode::Bake;
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
		PackageParams.HoudiniAssetName = FString();
		PackageParams.BakeFolder = InPackageFolder;//FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
		PackageParams.ObjectName = InPackageName;// FPaths::GetBaseFilename(InParent->GetName());

		if (NodeSyncOptions.bReplaceExisting)
		{
			PackageParams.ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;
		}
		else
		{
			PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;
		}

		// 5. Create all the objects using the outputs
		const FHoudiniStaticMeshGenerationProperties& StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
		const FMeshBuildSettings& MeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
		if (!HoudiniGeoImporter->CreateObjectsFromOutputs(NewOutputs, PackageParams, StaticMeshGenerationProperties, MeshBuildSettings))
			return FailImportAndReturn();

		// Get our result object and "finalize" them
		TArray<UObject*> Results = HoudiniGeoImporter->GetOutputObjects();
		for (UObject* Object : Results)
		{
			if (!IsValid(Object))
				continue;

			//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
			Object->MarkPackageDirty();
			Object->PostEditChange();
		}

		CleanUp();

		// Sync the content browser to the newly created assets
		if (GEditor)
			GEditor->SyncBrowserToObjects(Results);

		bSuccess = Results.Num() > 0;
	}
	else
	{
		// Spawn a new HoudiniActor with a HoudiniNodeSyncComponent
		AActor* CreatedActor = nullptr;

		// Clean up lambda
		auto CleanUp = [&CreatedActor]()
		{
			if (IsValid(CreatedActor))
			{
				CreatedActor->Destroy();
			}
		};

		// Failure lambda
		auto FailImportAndReturn = [this, &CleanUp, &CreatedActor]()
		{
			CleanUp();

			this->LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
			this->FetchStatusMessage = "Failed";
			this->FetchStatusDetails = "Houdini Node Sync - Fetch Failed.";

			// TODO: Improve me!
			FString Notification = TEXT("Houdini Node Sync - Fetch failed!");
			FHoudiniEngineUtils::CreateSlateNotification(Notification);

			return;
		};

		// Create a new HoudiniAssetActor
		// Get the asset Factory
		UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
		if (!Factory)
			return FailImportAndReturn();

		// Spawn in the current world/level
		ULevel* LevelToSpawnIn = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();
		CreatedActor = Factory->CreateActor(nullptr, LevelToSpawnIn, FHoudiniEngineEditorUtils::GetDefaulAssetSpawnTransform());
		if (!CreatedActor)
			return FailImportAndReturn();
		
		// Ensure spawn was successful
		AHoudiniAssetActor* HACActor = Cast<AHoudiniAssetActor>(CreatedActor);
		if (!IsValid(HACActor))
			return FailImportAndReturn();

		UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(HACActor->GetRootComponent());
		if (!IsValid(HAC))
			return FailImportAndReturn();

		// Remove the H logo here
		FHoudiniEngineUtils::RemoveHoudiniLogoFromComponent(HAC);

		// This will convert the HoudiniAssetActor to a NodeSync one
		HACActor->SetNodeSyncActor(true);
		
		// Check that we have a valid NodeSync component
		UHoudiniNodeSyncComponent* HNSC = Cast<UHoudiniNodeSyncComponent>(HACActor->GetHoudiniAssetComponent());
		if(!IsValid(HNSC))
			return FailImportAndReturn();

		// Add the Houdini logo back to the NodeSync component
		FHoudiniEngineUtils::AddHoudiniLogoToComponent(HNSC);

		// Set the Node Sync options
		// Fetch node path
		HNSC->SetFetchNodePath(FetchNodePath);
		HNSC->SetHoudiniAssetState(EHoudiniAssetState::NewHDA);

		// Disable proxies
		HNSC->bOverrideGlobalProxyStaticMeshSettings = true;
		HNSC->bEnableProxyStaticMeshOverride = false;
		//HNSC->StaticMeshMethod = EHoudiniStaticMeshMethod::FMeshDescription;

		// AutoBake?
		HNSC->SetBakeAfterNextCook(NodeSyncOptions.bAutoBake ? EHoudiniBakeAfterNextCook::Always : EHoudiniBakeAfterNextCook::Disabled);
		HNSC->bRemoveOutputAfterBake = true;

		// Other ptions
		HNSC->bUseOutputNodes = NodeSyncOptions.bUseOutputNodes;
		HNSC->bReplacePreviousBake = NodeSyncOptions.bReplaceExisting;
		HNSC->BakeFolder.Path = NodeSyncOptions.UnrealAssetFolder;
		HACActor->SetActorLabel(NodeSyncOptions.UnrealActorName);

		// Get the NodeId of the node we want to fetch
		HAPI_NodeId FetchNodeId;
		bSuccess = (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*HNSC->GetFetchNodePath()), &FetchNodeId));
		
		// Get its transform
		FTransform FetchTransform;
		if (FHoudiniEngineUtils::HapiGetAssetTransform(FetchNodeId, FetchTransform))
		{
			// Assign the transform to the actor
			HACActor->SetActorTransform(FetchTransform);
		}

		// Select the Actor we just created
		if (GEditor->CanSelectActor(CreatedActor, true, true))
		{
			GEditor->SelectNone(true, true, false);
			GEditor->SelectActor(CreatedActor, true, true, true);
		}

		// Update the status message to fetching
		this->LastFetchStatus = EHoudiniNodeSyncStatus::Running;
		this->FetchStatusMessage = "Fetching";
		this->FetchStatusDetails = "Houdini Node Sync - Fetching data from Houdini Node \"" + FetchNodePath + "\".";

		bSuccess = true;
	}

	if (bSuccess)
	{
		// TODO: Improve me!
		Notification = TEXT("Houdini Node Sync success!");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		LastFetchStatus = EHoudiniNodeSyncStatus::Success;
		FetchStatusMessage = "Fetch Success";
		FetchStatusDetails = "Houdini Node Sync - Succesfully fetched data from Houdini";
	}
	else
	{
		// TODO: Improve me!
		Notification = TEXT("Houdini Node Sync failed!");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Fetch Failed";
		FetchStatusDetails = "Houdini Node Sync - Failed fetching data from Houdini";
	}
}


bool
UHoudiniEditorNodeSyncSubsystem::GatherAllFetchNodeIds(
	HAPI_NodeId InFetchNodeId,
	const bool bUseOutputNodes,
	TArray<HAPI_NodeId>& OutOutputNodes)
{
	// This function behaves similarly to FHoudiniEngineUtils::GatherAllAssetOutputs()
	// With a few NodeSync specific twists:
	// - does not require an asset/asset info
	// - does not care about editable/templated nodes
	
	// Get the NodeInfo for the fetch node
	HAPI_NodeInfo FetchNodeInfo;
	FHoudiniApi::NodeInfo_Init(&FetchNodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InFetchNodeId, &FetchNodeInfo), false);

	// If the node is neither SOP nor OBJ, stop there
	if (FetchNodeInfo.type != HAPI_NODETYPE_SOP
		&& FetchNodeInfo.type != HAPI_NODETYPE_OBJ)
	{
		HOUDINI_LOG_ERROR(TEXT("Houdini Node Sync: Invalid fetch node type - the node should be either a SOP or OBJ node."));
		return false;
	}

	// If the node is a SOP instead of an OBJ/container node, then we won't need to run child queries on this node. They will fail.
	const bool bAssetHasChildren = !(FetchNodeInfo.type == HAPI_NODETYPE_SOP && FetchNodeInfo.childNodeCount == 0);
	
	// For non-container/subnet SOP nodes, no need to look further, just use the current SOP node
	if (!bAssetHasChildren)
	{
		OutOutputNodes.AddUnique(InFetchNodeId);
		return true;
	}

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	TArray<HAPI_Transform> ObjectTransforms;
	if (!FHoudiniEngineUtils::HapiGetObjectInfos(InFetchNodeId, ObjectInfos, ObjectTransforms))
	{
		HOUDINI_LOG_ERROR(TEXT("Houdini Node Sync: Fetch failed - Unable to get object infos for the node."));
		return false;
	}		

	bool bUseOutputFromSubnets = false;
	if (bAssetHasChildren && !FHoudiniEngineUtils::ContainsSopNodes(InFetchNodeId))
	{
		// Assume we're using a subnet-based HDA
		bUseOutputFromSubnets = true;
	}

	// Before we can perform visibility checks on the Object nodes, we have
	// to build a set of all the Object node ids. The 'AllObjectIds' act
	// as a visibility filter. If an Object node is not present in this
	// list, the content of that node will not be displayed (display / output / templated nodes).
	// NOTE that if the HDA contains immediate SOP nodes we will ignore
	// all subnets and only use the data outputs directly from the HDA.
	TSet<HAPI_NodeId> AllObjectIds;
	if (bUseOutputFromSubnets)
	{
		int NumObjSubnets;
		TArray<HAPI_NodeId> ObjectIds;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				InFetchNodeId,
				HAPI_NODETYPE_OBJ,
				HAPI_NODEFLAGS_OBJ_SUBNET,
				true,
				&NumObjSubnets
			),
			false);

		ObjectIds.SetNumUninitialized(NumObjSubnets);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				InFetchNodeId,
				ObjectIds.GetData(),
				NumObjSubnets
			),
			false);
		AllObjectIds.Append(ObjectIds);
	}
	else
	{
		AllObjectIds.Add(InFetchNodeId);
	}

	// Iterate through all objects to determine visibility and
	// gather output nodes that needs to be cooked.
	int32 OutputIdx = 1;
	const bool bIsSopAsset = FetchNodeInfo.type == HAPI_NODETYPE_SOP;
	for (int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ObjectIdx++)
	{
		// Retrieve the object info
		const HAPI_ObjectInfo& CurrentHapiObjectInfo = ObjectInfos[ObjectIdx];

		// Determine whether this object node is fully visible.
		bool bObjectIsVisible = false;
		HAPI_NodeId GatherOutputsNodeId = -1; // Outputs will be gathered from this node.
		if (!bAssetHasChildren)
		{
			// If the asset doesn't have children, we have to gather outputs from the asset's parent in order to output
			// this asset node
			bObjectIsVisible = true;
			GatherOutputsNodeId = FetchNodeInfo.parentId;
		}		
		else if (bIsSopAsset)
		{
			// When dealing with a SOP asset, be sure to gather outputs from the SOP node, not the
			// outer object node.
			bObjectIsVisible = true;
			GatherOutputsNodeId = InFetchNodeId;
		}		
		else
		{
			bObjectIsVisible = FHoudiniEngineUtils::IsObjNodeFullyVisible(AllObjectIds, InFetchNodeId, CurrentHapiObjectInfo.nodeId);
			GatherOutputsNodeId = CurrentHapiObjectInfo.nodeId;
		}

		// Build an array of the geos we'll need to process
		// In most case, it will only be the display geo
		TArray<HAPI_GeoInfo> GeoInfos;

		// These node ids may need to be cooked in order to extract part counts.
		TSet<HAPI_NodeId> CurrentOutGeoNodeIds;
		if (bObjectIsVisible)
		{
			// NOTE: The HAPI_GetDisplayGeoInfo will not always return the expected Geometry subnet's
			//     Display flag geometry. If the Geometry subnet contains an Object subnet somewhere, the
			//     GetDisplayGeoInfo will sometimes fetch the display SOP from within the subnet which is
			//     not what we want.

			// Resolve and gather outputs (display / output / template nodes) from the GatherOutputsNodeId.
			FHoudiniEngineUtils::GatherImmediateOutputGeoInfos(GatherOutputsNodeId,
				bUseOutputNodes,
				false,
				GeoInfos,
				CurrentOutGeoNodeIds);

		}

		// Add them to our global output node list
		for (const HAPI_NodeId& NodeId : CurrentOutGeoNodeIds)
		{
			OutOutputNodes.AddUnique(NodeId);
		}
	}

	return true;
}


FLinearColor
UHoudiniEditorNodeSyncSubsystem::GetStatusColor(const EHoudiniNodeSyncStatus& Status)
{
	FLinearColor OutStatusColor = FLinearColor::White;

	switch (Status)
	{
		case EHoudiniNodeSyncStatus::None:
			// Nothing done yet
			OutStatusColor = FLinearColor::White;
			break;

		case EHoudiniNodeSyncStatus::Failed:
			// Last operation failed
			OutStatusColor = FLinearColor::Red;
			break;

		case EHoudiniNodeSyncStatus::Success:
			// Last operation was successfull
			OutStatusColor = FLinearColor::Green;
			break;

		case EHoudiniNodeSyncStatus::SuccessWithErrors:
			// Last operation was succesfull, but reported errors
			OutStatusColor = FLinearColor(1.0f, 0.647f, 0.0f);
			break;

		case EHoudiniNodeSyncStatus::Running:
			// Fetching/Sending
			OutStatusColor = FLinearColor(0.0f, 0.749f, 1.0f);
			break;
		
		case EHoudiniNodeSyncStatus::Warning:
			// Display a warning
			OutStatusColor = FLinearColor(1.0f, 0.647f, 0.0f);
			break;
	}

	return OutStatusColor;
}