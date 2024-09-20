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
#include "SHoudiniNodeSyncPanel.h"
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
	// Register our extensions
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnRegisterLayoutExtensions().AddUObject(this, &UHoudiniEditorNodeSyncSubsystem::RegisterLayoutExtensions);

	// Initialize our input objects
	InitNodeSyncInputsIfNeeded();
}

void 
UHoudiniEditorNodeSyncSubsystem::Deinitialize()
{
	// Allow the inputs to delete their node
	NodeSyncWorldInput->SetCanDeleteHoudiniNodes(true);
	NodeSyncCBInput->SetCanDeleteHoudiniNodes(true);

	// Clean the world input
	FHoudiniInputTranslator::DisconnectAndDestroyInput(NodeSyncWorldInput, EHoudiniInputType::World);
	TArray<UHoudiniInputObject*>* InputObjects = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (InputObjects)
		InputObjects->Empty();
	
	// Clean the CB input
	FHoudiniInputTranslator::DisconnectAndDestroyInput(NodeSyncCBInput, EHoudiniInputType::World);
	InputObjects = NodeSyncCBInput->GetHoudiniInputObjectArray(EHoudiniInputType::Geometry);
	if (InputObjects)
		InputObjects->Empty();

	// Unregister our extensions
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
	// Do nothing if we have a valid session
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(FHoudiniEngine::Get().GetSession()))
		return FHoudiniEngine::Get().IsSessionSyncEnabled();

	// Attempt to open session sync
	if (!FHoudiniEngine::Get().IsSessionSyncEnabled())
	{
		FHoudiniEngineCommands::OpenSessionSync(true);
	}

	// Make sure we have a valid session
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(FHoudiniEngine::Get().GetSession()))
		return false;

	// returns true if we have a session sync-enabled session
	return FHoudiniEngine::Get().IsSessionSyncEnabled();
}


bool
UHoudiniEditorNodeSyncSubsystem::GetNodeSyncWorldInput(UHoudiniInput*& OutInput)
{
	if (!InitNodeSyncInputsIfNeeded())
		return false;

	if (!IsValid(NodeSyncWorldInput))
		return false;

	OutInput = NodeSyncWorldInput;

	return true;
}

bool
UHoudiniEditorNodeSyncSubsystem::GetNodeSyncCBInput(UHoudiniInput*& OutInput)
{
	if (!InitNodeSyncInputsIfNeeded())
		return false;

	if (!IsValid(NodeSyncCBInput))
		return false;

	OutInput = NodeSyncCBInput;

	return true;
}

bool
UHoudiniEditorNodeSyncSubsystem::InitNodeSyncInputsIfNeeded()
{
	if (IsValid(NodeSyncWorldInput) && IsValid(NodeSyncCBInput))
		return true;

	// Lambda used for initializing both NodeSyncInputs
	auto InitInputOptions = [&](UHoudiniInput* Input, const FString& InName, const bool& bIsWorld)
	{
		if (!IsValid(Input))
			return false;

		// Set the input type and name
		bool bOutBPModif = false;
		Input->SetInputType(bIsWorld ? EHoudiniInputType::World : EHoudiniInputType::Geometry, bOutBPModif);
		Input->SetName(InName);

		// Set the default input options
		Input->SetExportColliders(false);
		Input->SetExportLODs(false);
		Input->SetExportSockets(false);
		Input->SetLandscapeExportType(EHoudiniLandscapeExportType::Heightfield);
		Input->SetAddRotAndScaleAttributes(false);
		Input->SetImportAsReference(false);
		Input->SetImportAsReferenceRotScaleEnabled(false);
		Input->SetKeepWorldTransform(true);
		Input->SetPackBeforeMerge(false);
		Input->SetUnrealSplineResolution(50.0f);
		Input->SetExportLevelInstanceContent(true);
		Input->SetCanDeleteHoudiniNodes(false);
		Input->SetUseLegacyInputCurve(true);

		Input->SetAssetNodeId(-1);

		return true;
	};

	if (!IsValid(NodeSyncWorldInput))
	{
		// Create a fake HoudiniInput/HoudiniInputObject so we can use the input Translator to send the data to H
		FString InputObjectName = TEXT("NodeSyncWorldInput");
		NodeSyncWorldInput = NewObject<UHoudiniInput>(
			this, UHoudiniInput::StaticClass(), FName(*InputObjectName), RF_Transactional);

		if (!InitInputOptions(NodeSyncWorldInput, InputObjectName, true))
			return false;
	}

	if (!IsValid(NodeSyncCBInput))
	{
		// Create a fake HoudiniInput/HoudiniInputObject so we can use the input Translator to send the data to H
		FString InputObjectName = TEXT("NodeSyncCBInput");
		NodeSyncCBInput = NewObject<UHoudiniInput>(
			this, UHoudiniInput::StaticClass(), FName(*InputObjectName), RF_Transactional);

		if (!InitInputOptions(NodeSyncCBInput, InputObjectName, false))
			return false;
	}

	return true;
}


void
UHoudiniEditorNodeSyncSubsystem::SendContentBrowserSelection(const TArray<UObject*>& CurrentCBSelection)
{
	LastSendStatus = EHoudiniNodeSyncStatus::Running;
	SendStatusMessage = "Sending...";

	if (CurrentCBSelection.Num() <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Node Sync: No selection in the content browser"));

		LastSendStatus = EHoudiniNodeSyncStatus::Failed;
		SendStatusMessage = "Send Failed: No selection in the content browser.";
		SendStatusDetails = "Houdini Node Sync - Send Failed: No selection in the content browser\nPlease select Assets in the content browser and try again.";

		return;
	}

	// No need to upload something we've already sent ... 
	TArray<UObject*> ObjectsToSend = CurrentCBSelection;
	TArray<UHoudiniInputObject*>* InputObjects = NodeSyncCBInput->GetHoudiniInputObjectArray(EHoudiniInputType::Geometry);
	if (InputObjects)
	{
		// ... so remove all selected objects that were previously sent
		for (const auto& CurInputObject : *InputObjects)
		{
			UObject* CurrentObject = CurInputObject ? CurInputObject->GetObject() : nullptr;
			if (!CurrentObject)
				continue;

			int32 FoundIdx = ObjectsToSend.Find(CurrentObject);
			if (FoundIdx != INDEX_NONE)
				ObjectsToSend.RemoveAt(FoundIdx);
		}
	}

	// Keep track of the index where we add new things
	// New objects are appended at the end, and we don't want to resend the whole array
	int32 AddedObjectIndex = NodeSyncCBInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);

	SendToHoudini(ObjectsToSend, AddedObjectIndex, false);
}


void
UHoudiniEditorNodeSyncSubsystem::SendWorldSelection()
{
	LastSendStatus = EHoudiniNodeSyncStatus::Running;
	SendStatusMessage = "Sending...";

	// Get current world selection
	TArray<UObject*> CurrentWorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(CurrentWorldSelection, false);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Node Sync: No selection in the world outliner"));

		LastSendStatus = EHoudiniNodeSyncStatus::Failed;
		SendStatusMessage = "Send Failed: No selection in the world outliner.";
		SendStatusDetails = "Houdini Node Sync - Send Failed: No selection in the world outliner\nPlease select Actors in the World and try again.";

		return;
	}

	// Ensure that the NodeSync inputs are valid
	InitNodeSyncInputsIfNeeded();

	// No need to upload something we've already sent
	TArray<UHoudiniInputObject*>* InputObjects = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (InputObjects)
	{
		// Remove all selected objects that were previously sent
		for (const auto& CurInputObject : *InputObjects)
		{
			UObject* CurrentObject = CurInputObject ? CurInputObject->GetObject() : nullptr;
			if (!CurrentObject)
				continue;

			int32 FoundIdx = CurrentWorldSelection.Find(CurrentObject);
			if (FoundIdx != INDEX_NONE)
				CurrentWorldSelection.RemoveAt(FoundIdx);
		}
	}

	// Keep track of the index where we add new things
	// New objects are appended at the end, and we don't want to resend the whole array
	int32 AddedObjectIndex = NodeSyncWorldInput->GetNumberOfInputObjects(EHoudiniInputType::World);

	// See if our previously sent nodes are still valid
	if (CheckNodeSyncInputNodesValid())
		UpdateNodeSyncInputs();	

	// Send the selected data to Houdini
	SendToHoudini(CurrentWorldSelection, AddedObjectIndex, true);

	// Rebuild the NodeSync selection view
	FHoudiniEngineEditor::Get().GetNodeSyncPanel()->RebuildSelectionView();
}


void 
UHoudiniEditorNodeSyncSubsystem::SendToHoudini(const TArray<UObject*>& SelectedAssets, int32 ObjectIndex, const bool& bSendWorld)
{
	if (SelectedAssets.Num() <= 0)
	{
		LastSendStatus = EHoudiniNodeSyncStatus::Success;
		SendStatusMessage = "Send Success";
		SendStatusDetails = "Houdini Node Sync - Send success - No new data to be sent was found!";
		return;
	}

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
	// As a subnet, so it's able to contain multiple geos
	FString SendNodePath = NodeSyncOptions.SendNodePath;
	HAPI_NodeId  UnrealContentNodeId = -1;
	HAPI_Result result = FHoudiniApi::GetNodeFromPath(
		FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*SendNodePath), &UnrealContentNodeId);

	if ((result != HAPI_RESULT_SUCCESS) || (UnrealContentNodeId < 0))
	{
		FString Name = SendNodePath;
		Name.RemoveFromStart("/obj/");
		result = FHoudiniEngineUtils::CreateNode( - 1, "Object/subnet", TCHAR_TO_ANSI(*Name), true, &UnrealContentNodeId);
	}

	// Decide wether we want to use to World or CB Input
	UHoudiniInput* NodeSyncInput = bSendWorld ? NodeSyncWorldInput : NodeSyncCBInput;

	// Make sure the NodeSync input have been initialized
	if (!InitNodeSyncInputsIfNeeded() || !IsValid(NodeSyncInput))
	{
		// For now, just warn the session is not session sync
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: the current session is not session-sync one!"));

		LastSendStatus = EHoudiniNodeSyncStatus::Failed;
		SendStatusMessage = "Error: Unable to initialize/access the Node Sync Input!";
		SendStatusDetails = SendStatusMessage;

		return;	
	}

	// Default input options
	NodeSyncInput->SetCanDeleteHoudiniNodes(false);
	NodeSyncInput->SetUseLegacyInputCurve(true);
		
	// TODO: Check?
	NodeSyncInput->SetAssetNodeId(-1);
	NodeSyncInput->SetInputNodeId(UnrealContentNodeId);

	const FHoudiniInputObjectSettings InputSettings(NodeSyncInput);

	// For each selected Asset, create a HoudiniInputObject and send it to H
	for (int32 Idx = 0; Idx < SelectedAssets.Num(); Idx++)
	{
		TArray<int32> CreatedNodeIds;
		TSet<FUnrealObjectInputHandle> Handles;
		UObject* CurrentObject = SelectedAssets[Idx];
		if (!IsValid(CurrentObject))
			continue;

		/*
		// Create an input object wrapper for the current object
		UHoudiniInputObject * CurrentInputObject = UHoudiniInputObject::CreateTypedInputObject(CurrentObject, NodeSyncInput, FString::FromInt(Idx + 1), InputSettings);
		if (!IsValid(CurrentInputObject))
			continue;
		*/

		NodeSyncInput->SetInputObjectAt(ObjectIndex + Idx, CurrentObject);

		UHoudiniInputObject* CurrentInputObject = NodeSyncInput->GetHoudiniInputObjectAt(ObjectIndex + Idx);
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

		// See first if the node already exists
		HAPI_NodeId CurrentObjectNodeId = -1;
		result = FHoudiniApi::GetNodeFromPath(
			FHoudiniEngine::Get().GetSession(),	UnrealContentNodeId, TCHAR_TO_ANSI(*ObjectName), &CurrentObjectNodeId);

		if ((result != HAPI_RESULT_SUCCESS) || (CurrentObjectNodeId < 0))
		{
			// No existing node found - create a new one
			if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::CreateNode(
				UnrealContentNodeId, "geo", TCHAR_TO_ANSI(*ObjectName), true, &CurrentObjectNodeId))
			{
				HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to create input object geo node for %s."), *CurrentInputObject->GetName());

				LastSendStatus = EHoudiniNodeSyncStatus::SuccessWithErrors;
				SendStatusMessage = "Send Success with errors";
				SendStatusDetails = "Houdini Node Sync - Send success with errors - Not all selected objects were created.";
			}
		}

		// Preset the existing Object Node ID to the unreal content node
		CurrentInputObject->SetInputNodeId(-1);
		CurrentInputObject->SetInputObjectNodeId(CurrentObjectNodeId);

		// TODO: Transform for actors?
		FTransform CurrentActorTransform = FTransform::Identity;

		// Send the HoudiniInputObject to H
		if (!FHoudiniInputTranslator::UploadHoudiniInputObject(
			NodeSyncInput, CurrentInputObject, CurrentActorTransform, CreatedNodeIds, Handles, false))
		{
			HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to send %s to %s."), *CurrentInputObject->GetName(), *SendNodePath);

			LastSendStatus = EHoudiniNodeSyncStatus::SuccessWithErrors;
			SendStatusMessage = "Send Success with errors";
			SendStatusDetails = "Houdini Node Sync - Send success with errors - Not all selected objects were created.";

			continue;
		}

		// Mark that input object as non dirty
		CurrentInputObject->MarkChanged(false);
		CurrentInputObject->MarkTransformChanged(false);

		// We've created the input nodes for this object, now, we need to object merge them into the content node in the path specified by the user
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

	// Start ticking if needed once we send something
	if(NodeSyncOptions.bSyncWorldInput)
		StartTicking();
}


void
UHoudiniEditorNodeSyncSubsystem::UpdateAllSelection()
{
	LastSendStatus = EHoudiniNodeSyncStatus::Running;
	SendStatusMessage = "Updating...";

	// Get current world selection
	TArray<UObject*> CurrentWorldSelection;

	// Make sure the node sync inputs are valid
	InitNodeSyncInputsIfNeeded();

	// Build an array of all previously sent actors
	TArray<UHoudiniInputObject*>* InputObjects = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (InputObjects)
	{
		for (const auto& CurInputObject : *InputObjects)
		{
			CurrentWorldSelection.Add(CurInputObject->GetObject());
		}
	}

	// Resend all the WorldSelection
	SendToHoudini(CurrentWorldSelection, 0, true);

	// Rebuild the NodeSync selection view
	FHoudiniEngineEditor::Get().GetNodeSyncPanel()->RebuildSelectionView();
}

void
UHoudiniEditorNodeSyncSubsystem::DeleteAllSelection()
{
	LastSendStatus = EHoudiniNodeSyncStatus::Running;
	SendStatusMessage = "Deleting...";

	// Make sure the node sync input is valid
	InitNodeSyncInputsIfNeeded();

	// Shortly authorize the input to delete its node
	NodeSyncWorldInput->SetCanDeleteHoudiniNodes(true);

	// Clean the world input
	bool bReturn = FHoudiniInputTranslator::DisconnectAndDestroyInput(NodeSyncWorldInput, EHoudiniInputType::World);
	TArray<UHoudiniInputObject*>* InputObjects = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (InputObjects)
		InputObjects->Empty();

	NodeSyncWorldInput->SetCanDeleteHoudiniNodes(false);

	if (bReturn)
	{
		LastSendStatus = EHoudiniNodeSyncStatus::Success;
		SendStatusMessage = "Delete Success.";
		SendStatusDetails = "Houdini Node Sync - Delete Success: Successfully deleted all sent data!";
	}
	else
	{
		LastSendStatus = EHoudiniNodeSyncStatus::Failed;
		SendStatusMessage = "Delete Failed.";
		SendStatusDetails = "Houdini Node Sync - Delete Failed: Unable to delete all sent data!";
	}

	// Rebuild the NodeSync selection view
	FHoudiniEngineEditor::Get().GetNodeSyncPanel()->RebuildSelectionView();
}



void 
UHoudiniEditorNodeSyncSubsystem::FetchFromHoudini()
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

	// We use the BGEO importer when Fetching to the content browser
	bool bUseBGEOImport = !NodeSyncOptions.bFetchToWorld;
	bool bSuccess = false;

	// Parse the fetch node path into a string array
	// Multiple paths are separated by ;
	TArray<FString> FetchNodePaths;
	NodeSyncOptions.FetchNodePath.ParseIntoArray(FetchNodePaths, TEXT(";"), true);

	for (int32 PathIdx = 0; PathIdx < FetchNodePaths.Num(); PathIdx++)
	{
		// Only use the first one for now
		FString CurrentFetchNodePath = FetchNodePaths[PathIdx];

		// Make sure that the FetchNodePath is a valid Houdini node path pointing to a valid Node
		HAPI_NodeId  FetchNodeId = -1;
		if (!ValidateFetchedNodePath(CurrentFetchNodePath, FetchNodeId))
			return;

		if (bUseBGEOImport)
		{
			// We need to gather all the required nodes
			TArray<HAPI_NodeId> FetchNodeIds;
			if (!GatherAllFetchedNodeIds(FetchNodeId, NodeSyncOptions.bUseOutputNodes, FetchNodeIds))
			{
				HOUDINI_LOG_ERROR(TEXT("Houdini Node Sync: Failed to gather fetch nodes."));
				LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
				FetchStatusMessage = "Failed: Unable to gather fetch node outputs!";
				FetchStatusDetails = "Houdini Node Sync - Fetch Failed - Unable to gather fetch node outputs.";

				return;
			}

			// Make sure that all the required output nodes have been cooked
			// This ensure that we'll be able to get the proper number of parts for them
			HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
			for (auto CurrentNodeId : FetchNodeIds)
			{
				if (!FHoudiniEngineUtils::HapiCookNode(CurrentNodeId, &CookOptions, true))
				{
					HOUDINI_LOG_ERROR(TEXT("Failed to cook NodeSyncFetch node!"));
					// Only log? still try to continue with the output processing?
					// return;
				}
			}

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
			PackageParams.HoudiniAssetName = NodeSyncOptions.GetFetchNodeNameAt(PathIdx);
			PackageParams.BakeFolder = NodeSyncOptions.UnrealAssetFolder;

			PackageParams.ObjectName = NodeSyncOptions.GetUnrealAssetName(PathIdx);
			PackageParams.HoudiniAssetActorName = NodeSyncOptions.GetUnrealActorLabel(PathIdx);
			PackageParams.NameOverride = NodeSyncOptions.GetUnrealAssetName(PathIdx);

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

			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (NodeSyncOptions.bReplaceExisting)
			{
				// See if an Actor already exist for this node path
				AHoudiniAssetActor* FoundActor = nullptr;
				for (TActorIterator<AHoudiniAssetActor> ActorIt(World, AHoudiniAssetActor::StaticClass(), EActorIteratorFlags::OnlyActiveLevels); ActorIt; ++ActorIt)
				{
					FoundActor = *ActorIt;
					if (!FoundActor)
						continue;

					UHoudiniNodeSyncComponent* FoundHNSC = Cast<UHoudiniNodeSyncComponent>(FoundActor->GetHoudiniAssetComponent());
					if (!IsValid(FoundHNSC))
						continue;

					if (FoundHNSC->GetFetchNodePath() != CurrentFetchNodePath)
						continue;

					// Re-use the found actor
					CreatedActor = FoundActor;
				}
			}

			if (!IsValid(CreatedActor))
			{
				// We need to create a new HoudiniAssetActor
				// Get the asset Factory
				UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
				if (!Factory)
					return FailImportAndReturn();

				// Spawn in the current world/level
				ULevel* LevelToSpawnIn = World->GetCurrentLevel();
				CreatedActor = Factory->CreateActor(nullptr, LevelToSpawnIn, FHoudiniEngineEditorUtils::GetDefaulAssetSpawnTransform());
			}

			if (!IsValid(CreatedActor))
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
			HNSC->SetFetchNodePath(CurrentFetchNodePath);
			HNSC->SetHoudiniAssetState(EHoudiniAssetState::NewHDA);

			// Disable proxies
			HNSC->bOverrideGlobalProxyStaticMeshSettings = true;
			HNSC->bEnableProxyStaticMeshOverride = false;
			//HNSC->StaticMeshMethod = EHoudiniStaticMeshMethod::FMeshDescription;

			// AutoBake?
			HNSC->SetBakeAfterNextCook(NodeSyncOptions.bAutoBake ? EHoudiniBakeAfterNextCook::Always : EHoudiniBakeAfterNextCook::Disabled);
			HNSC->bRemoveOutputAfterBake = true;

			// Other options
			HNSC->bUseOutputNodes = NodeSyncOptions.bUseOutputNodes;
			HNSC->bReplacePreviousBake = NodeSyncOptions.bReplaceExisting;
			HNSC->BakeFolder.Path = NodeSyncOptions.UnrealAssetFolder;

			// Make sure we the actor has a unique name/label
			FString ActorNameAndLabel = NodeSyncOptions.GetUnrealActorLabel(PathIdx);
			// Try to find an existing actor of the desired name - make our name unique if we find one
			AActor* NamedActor = FHoudiniEngineUtils::FindActorInWorldByLabelOrName<AActor>(World, ActorNameAndLabel);
			if(NamedActor == nullptr)
				HACActor->SetActorLabel(NodeSyncOptions.GetUnrealActorLabel(PathIdx));
			else if(NamedActor != HACActor)
				FHoudiniEngineUtils::RenameToUniqueActor(HACActor, NodeSyncOptions.GetUnrealActorLabel(PathIdx));

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
			this->FetchStatusDetails = "Houdini Node Sync - Fetching data from Houdini Node \"" + CurrentFetchNodePath + "\".";

			bSuccess = true;
		}
	}

	if (bSuccess)
	{
		// TODO: Improve me!
		Notification = TEXT("Houdini Node Sync success!");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);

		LastFetchStatus = EHoudiniNodeSyncStatus::Success;
		FetchStatusMessage = "Fetch Success";
		FetchStatusDetails = "Houdini Node Sync - Successfully fetched data from Houdini";
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
UHoudiniEditorNodeSyncSubsystem::GatherAllFetchedNodeIds(
	HAPI_NodeId InFetchNodeId,
	const bool bUseOutputNodes,
	TArray<HAPI_NodeId>& OutOutputNodes)
{
	// This function behaves similarly to FHoudiniEngineUtils::GatherAllAssetOutputs()
	// With a few NodeSync specific twists:
	// - does not require an asset/asset info
	// - does not care about editable/templated nodes
	// - ignore object visibility
	
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
				HAPI_NODEFLAGS_OBJ_SUBNET | HAPI_NODEFLAGS_NON_BYPASS,
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
		HAPI_NodeId GatherOutputsNodeId = -1; // Outputs will be gathered from this node.
		if (!bAssetHasChildren)
		{
			// If the asset doesn't have children, we have to gather outputs from the asset's parent in order to output
			// this asset node
			GatherOutputsNodeId = FetchNodeInfo.parentId;
		}
		else if (bIsSopAsset)
		{
			// When dealing with a SOP asset, be sure to gather outputs from the SOP node, not the
			// outer object node.
			GatherOutputsNodeId = InFetchNodeId;
		}
		else
		{
			GatherOutputsNodeId = CurrentHapiObjectInfo.nodeId;
		}

		// Build an array of the geos we'll need to process
		// In most case, it will only be the display geo
		TArray<HAPI_GeoInfo> GeoInfos;

		// These node ids may need to be cooked in order to extract part counts.
		TSet<HAPI_NodeId> CurrentOutGeoNodeIds;
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


bool
UHoudiniEditorNodeSyncSubsystem::ValidateFetchedNodePath(const FString& InFetchNodePath, HAPI_NodeId& OutFetchedNodeId)
{
	// Make sure that the FetchNodePath is a valid Houdini node path pointing to a valid Node
	OutFetchedNodeId = -1;
	
	// Make sure we're not trying to fetch /obj, as this seems to crash HE	
	if (InFetchNodePath.Equals("/obj", ESearchCase::IgnoreCase)
		|| InFetchNodePath.Equals("/obj/", ESearchCase::IgnoreCase))
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FetchNodePath"));
		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Failed: Invalid Fetch node path!";
		FetchStatusDetails = "Houdini Node Sync - Fetch Failed - Unable to fetch /obj/.";

		return false;
	}

	// Get the node ID for the given path
	HAPI_Result Result = FHoudiniApi::GetNodeFromPath(
		FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*InFetchNodePath), &OutFetchedNodeId);
	if ((Result != HAPI_RESULT_SUCCESS) || (OutFetchedNodeId < 0))
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FetchNodePath"));
		LastFetchStatus = EHoudiniNodeSyncStatus::Failed;
		FetchStatusMessage = "Failed: Invalid Fetch node path!";
		FetchStatusDetails = "Houdini Node Sync - Fetch Failed - The Fetch node path is invalid.";

		return false;
	}

	return true;
}


bool
UHoudiniEditorNodeSyncSubsystem::CheckNodeSyncInputNodesValid()
{
	if (!IsValid(NodeSyncWorldInput))
		return false;

	// No need to tick if we dont have any input objects
	if (NodeSyncWorldInput->GetNumberOfInputObjects(EHoudiniInputType::World) <= 0)
	{
		StopTicking();
		return false;
	}

	const TArray<UHoudiniInputObject*>* InputObjects = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (!InputObjects)
		return false;

	bool bReturn = false;
	for (const auto& CurInputObject : *InputObjects)
	{
		if (FHoudiniEngineUtils::IsHoudiniNodeValid(CurInputObject->GetInputObjectNodeId()))
			continue;

		CurInputObject->MarkChanged(true);
		bReturn = true;
	}

	return bReturn;
}

bool
UHoudiniEditorNodeSyncSubsystem::UpdateNodeSyncInputs()
{
	if (!IsValid(NodeSyncWorldInput))
		return false;

	// No need to tick if we dont have any input objects
	if (NodeSyncWorldInput->GetNumberOfInputObjects(EHoudiniInputType::World) <= 0)
	{		
		StopTicking();
		return true;
	}

	// See if we need to update some of the node sync inputs
	if (!FHoudiniInputTranslator::UpdateWorldInput(NodeSyncWorldInput))
		return false;

	if (!NodeSyncWorldInput->NeedsToTriggerUpdate())
		return false;

	bool bSuccess = true;
	if (NodeSyncWorldInput->IsDataUploadNeeded())
	{
		TArray<UHoudiniInputObject*>* InputObjectsArray = NodeSyncWorldInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);

		// Iterate on all the input objects and see if they need to be uploaded
		TArray<int32> CreatedNodeIds;
		TSet<FUnrealObjectInputHandle> Handles;
		TArray<int32> ValidNodeIds;
		TArray<UHoudiniInputObject*> ChangedInputObjects;
		for (int32 ObjIdx = 0; ObjIdx < InputObjectsArray->Num(); ObjIdx++)
		{
			UHoudiniInputObject* CurrentInputObject = (*InputObjectsArray)[ObjIdx];
			if (!IsValid(CurrentInputObject))
				continue;

			ValidNodeIds.Reset();
			ChangedInputObjects.Reset();
			// The input object could have child objects: GetChangedObjectsAndValidNodes finds if the object itself or
			// any its children has changed, and also returns the NodeIds of those objects that are still valid and
			// unchanged
			CurrentInputObject->GetChangedObjectsAndValidNodes(ChangedInputObjects, ValidNodeIds);

			// Keep track of the node ids for unchanged objects that already exist
			if (ValidNodeIds.Num() > 0)
				CreatedNodeIds.Append(ValidNodeIds);

			// Upload the changed input objects
			for (UHoudiniInputObject* ChangedInputObject : ChangedInputObjects)
			{
				// Upload the current input object to Houdini
				if (!FHoudiniInputTranslator::UploadHoudiniInputObject(
					NodeSyncWorldInput, ChangedInputObject, FTransform::Identity, CreatedNodeIds, Handles, ChangedInputObject->CanDeleteHoudiniNodes()))
					bSuccess = false;
			}
		}

		/*
		// We've created the input nodes for this object, now, we need to object merge them into the content node in the path specified by the user
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
		*/

		NodeSyncWorldInput->MarkDataUploadNeeded(!bSuccess);
	}

	if (NodeSyncWorldInput->IsTransformUploadNeeded())
	{
		bSuccess &= FHoudiniInputTranslator::UploadInputTransform(NodeSyncWorldInput);
	}

	// Update the input properties AFTER eventually uploading it
	bSuccess = FHoudiniInputTranslator::UpdateInputProperties(NodeSyncWorldInput);

	if (bSuccess)
	{
		NodeSyncWorldInput->MarkChanged(false);
		NodeSyncWorldInput->MarkAllInputObjectsChanged(false);
	}

	if (NodeSyncWorldInput->HasInputTypeChanged())
		NodeSyncWorldInput->SetPreviousInputType(EHoudiniInputType::Invalid);

	// Even if we failed, no need to try updating again.
	NodeSyncWorldInput->SetNeedsToTriggerUpdate(false);

	return true;
}

void
UHoudiniEditorNodeSyncSubsystem::StartTicking()
{
	// If we have no timer delegate spawned, spawn one.
	if (!TickerHandle.IsValid() && GEditor)
	{
		// We use the ticker manager so we get ticked once per frame, no more.
		//TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &UHoudiniEditorNodeSyncSubsystem::Tick));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) { return this->Tick(DeltaTime); }));

		// Grab current time for delayed notification.
		//FHoudiniEngine::Get().SetHapiNotificationStartedTime(FPlatformTime::Seconds());
	}

	dLastTick = 0.0;
}

void
UHoudiniEditorNodeSyncSubsystem::StopTicking()
{
	if (TickerHandle.IsValid() && GEditor)
	{
		if (IsInGameThread())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();

			bMustStopTicking = false;
			dLastTick = 0.0;
		}
		else
		{
			// We can't stop ticking now as we're not in the game Thread,
			// and accessing the timer would crash, indicate that we want to stop ticking asap
			// This can happen when loosing a session due to a Houdini crash
			bMustStopTicking = true;
		}
	}
}

bool 
UHoudiniEditorNodeSyncSubsystem::IsTicking() const
{
	return TickerHandle.IsValid();
}

bool
UHoudiniEditorNodeSyncSubsystem::Tick(float DeltaTime)
{
	if (bMustStopTicking)
	{
		// Ticking should be stopped immediately
		StopTicking();
		return true;
	}

	double dNow = FPlatformTime::Seconds();
	if ((dNow - dLastTick) < 1.0)
		return true;

	UpdateNodeSyncInputs();

	dLastTick = dNow;

	return true;
}