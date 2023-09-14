// Fill out your copyright notice in the Description page of Project Settings.


#include "HoudiniEditorSubsystem.h"

#include "HoudiniEngine.h"
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
#include "HoudiniOutputTranslator.h"
#include "UnrealMeshTranslator.h"

#include "Engine/SkeletalMesh.h"
//#include "Toolkits/AssetEditorModeUILayer.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif


bool 
UHoudiniEditorSubsystem::CreateSessionIfNeeded()
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
UHoudiniEditorSubsystem::SendStaticMeshToHoudini(
	const HAPI_NodeId& InMeshNodeId, UStaticMesh* InStaticMesh)
{
	if (!IsValid(InStaticMesh))
		return false;
  
    int32 LODIndex = 0;
    FStaticMeshSourceModel& SrcModel = InStaticMesh->GetSourceModel(LODIndex);
    
    FMeshDescription* MeshDesc = InStaticMesh->GetMeshDescription(LODIndex);
	if (!MeshDesc)
		return false;

	if (!FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
		InMeshNodeId, *MeshDesc, LODIndex, false, false, InStaticMesh, nullptr))
		return false;

    //bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(SM, InObject->InputNodeId, SMCName, SMC, bExportLODs, bExportSockets, bExportColliders);
   
	// Set the display flag
    FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), InMeshNodeId, 1);

	// Commit the geo
    if(HAPI_RESULT_SUCCESS != FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), InMeshNodeId))
		return false;

	return true;
}


bool 
UHoudiniEditorSubsystem::SendSkeletalMeshToHoudini(
	const HAPI_NodeId& InNodeId, USkeletalMesh* InSkelMesh)
{
	if (!IsValid(InSkelMesh))
		return false;

	int32 LODIndex = 0;
	HAPI_NodeId SKNodeId = InNodeId;
	if (!FUnrealMeshTranslator::SetSkeletalMeshDataOnNode(InSkelMesh, nullptr, SKNodeId, LODIndex, false, false))
		return false;

	// Set the display flag
	FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), SKNodeId, 1);

    if(HAPI_RESULT_SUCCESS != FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), SKNodeId))
		return false;

	return true;
}



bool
UHoudiniEditorSubsystem::GetNodeSyncInput(UHoudiniInput*& OutInput)
{
	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniEditorSubsystem))
		return false;

	if (!HoudiniEditorSubsystem->InitNodeSyncInputIfNeeded())
		return false;

	if (!IsValid(HoudiniEditorSubsystem->NodeSyncInput))
		return false;

	OutInput = HoudiniEditorSubsystem->NodeSyncInput;

	return true;
}


bool
UHoudiniEditorSubsystem::InitNodeSyncInputIfNeeded()
{
	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniEditorSubsystem))
		return false;

	UHoudiniInput*& NSInput = HoudiniEditorSubsystem->NodeSyncInput;
	if (IsValid(NSInput))
		return true;

	// Create a fake HoudiniInput/HoudiniInputObject so we can use the input Translator to send the data to H
	FString InputObjectName = TEXT("NodeSyncInput");
	NSInput = NewObject<UHoudiniInput>(
		HoudiniEditorSubsystem, UHoudiniInput::StaticClass(), FName(*InputObjectName), RF_Transactional);

	//NodeSyncInput->AddToRoot();

	if (!IsValid(NSInput))
	{
		// TODO: Always call remove from root, even for failures! (use a lambda for returns)
		NSInput->RemoveFromRoot();
		return false;
	}

	// Set the default input options
	// TODO: Fill those from the NodeSync UI?!
	NSInput->SetExportColliders(false);
	NSInput->SetExportLODs(false);
	NSInput->SetExportSockets(false);
	NSInput->SetLandscapeExportType(EHoudiniLandscapeExportType::Heightfield);
	NSInput->SetAddRotAndScaleAttributes(false);
	NSInput->SetImportAsReference(false);
	NSInput->SetImportAsReferenceRotScaleEnabled(false);
	NSInput->SetKeepWorldTransform(true);
	NSInput->SetPackBeforeMerge(false);
	NSInput->SetUnrealSplineResolution(50.0f);

	// default input options
	NSInput->SetCanDeleteHoudiniNodes(false);
	NSInput->SetUseLegacyInputCurve(true);

	// TODO: Check?
	NSInput->SetAssetNodeId(-1);
	//NSInput->SetInputNodeId(UnrealContentNodeId);

	// Input type? switch to world if actors in the selection ?
	bool bOutBPModif = false;
	NSInput->SetInputType(EHoudiniInputType::Geometry, bOutBPModif);
	NSInput->SetName(TEXT("NodeSyncInput"));

	return true;
}


void
UHoudiniEditorSubsystem::SendWorldSelection()
{
	UHoudiniEditorSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniSubsystem))
		return;

	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, false);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Node Sync: No selection in the world outliner"));
		return;
	}

	// Input type? switch to world when sending from world? (necessary?)
	//bool bOutBPModif = false;
	//HoudiniSubsystem->NodeSyncInput->SetInputType(EHoudiniInputType::World, bOutBPModif);

	HoudiniSubsystem->SendToHoudini(WorldSelection);
}


void 
UHoudiniEditorSubsystem::SendToHoudini(const TArray<UObject*>& SelectedAssets)
{
	if (SelectedAssets.Num() <= 0)
		return;

	// Add a slate notification
	FString Notification = TEXT("Sending selected assets to Houdini...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Sending To Houdini!"));

	if (!CreateSessionIfNeeded())
	{
		// For now, just warn the session is not session sync
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: the current session is not session-sync one!"));
	}

	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniEditorSubsystem))
		return;

	bool bUseInput = true;
	if (!bUseInput)
	{
		// Previous method, not using inputs
		HAPI_Result result = HAPI_RESULT_FAILURE;

		// Create the content node if needed
		if (object_node_id < 1)
		{			
			HAPI_NodeId  UnrealContentNode = -1;
			result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, "Object/geo", "UnrealContent", true, &UnrealContentNode);
			HAPI_NodeInfo MyGeoNodeInfo = FHoudiniApi::NodeInfo_Create();
			result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), UnrealContentNode, &MyGeoNodeInfo);

			object_node_id = UnrealContentNode;
		}

		FString SendNodePath = HoudiniEditorSubsystem->NodeSyncOptions.SendNodePath;
				
		HAPI_NodeId  UnrealContentNodeId = -1;
		result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*SendNodePath), &UnrealContentNodeId);
		if ((result == HAPI_RESULT_SUCCESS) && (UnrealContentNodeId >= 0))
		{
			//HOUDINI_LOG_MESSAGE(TEXT("Sending Node To %s "), *SendNodePath);
			HOUDINI_LOG_MESSAGE(TEXT("Sending Node"));
		}
		else
		{
			FString Name = SendNodePath;
			Name.RemoveFromStart("/obj/");
			result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, "Object/geo", TCHAR_TO_ANSI(*Name), true, &UnrealContentNodeId);
		}

		HAPI_NodeId mesh_node_id = -1;
		USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(SelectedAssets[0]);
		if (SkelMesh)
		{
			FString SKName = TEXT("SkeletalMesh_") + SkelMesh->GetName();
			result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), UnrealContentNodeId, "null", TCHAR_TO_ANSI(*SKName), true, &mesh_node_id);
			SendSkeletalMeshToHoudini(mesh_node_id, SkelMesh);
		}
		else
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(SelectedAssets[0]);
			if (StaticMesh)
			{
				FString SKName = TEXT("StaticMesh_") + StaticMesh->GetName();
				result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), UnrealContentNodeId, "null", TCHAR_TO_ANSI(*SKName), true, &mesh_node_id);
				SendStaticMeshToHoudini(mesh_node_id, StaticMesh);
			}
		}
	}
	else
	{
		// Create the content node
		// As as subnet, so it's able to contain multiple geos
		FString SendNodePath = HoudiniEditorSubsystem->NodeSyncOptions.SendNodePath;
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
		if (!HoudiniEditorSubsystem->GetNodeSyncInput(NSInput))
		{
			// TODO: Always call remove from root, even for failures! (use a lambda for returns)
			//NSInput->RemoveFromRoot();
			return;
		}

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
			UObject* CurrentObject = SelectedAssets[Idx];
			if (!IsValid(CurrentObject))
				continue;

			// Create an input object wrapper for the current object
			UHoudiniInputObject * CurrentInputObject = UHoudiniInputObject::CreateTypedInputObject(CurrentObject, NSInput, FString::FromInt(Idx + 1), InputSettings);
			if (!IsValid(CurrentInputObject))
				continue;

			// Create a geo node for this object in the content node
			FString ObjectName = CurrentObject->GetName();

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
			}

			// Preset the existing Object Node ID to the unreal content node
			// !! Do not preset, as the input system will destroy those previous input nodes!
			CurrentInputObject->SetInputNodeId(-1);
			CurrentInputObject->SetInputObjectNodeId(-1);
			
			// TODO: Transform for actors?
			FTransform CurrentActorTransform = FTransform::Identity;

			// Send the HoudiniInputObject to H
			if (!FHoudiniInputTranslator::UploadHoudiniInputObject(
				NSInput, CurrentInputObject, CurrentActorTransform, CreatedNodeIds, false))
			{
				HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to send %s to %s."), *CurrentInputObject->GetName(), *SendNodePath);
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

		// TODO: Always call remove from root, even for failures! (use a lambda for returns)
		//NSInput->RemoveFromRoot();
	}

	// TODO: Improve me! handle failures!
	Notification = TEXT("Houdini Node Sync success!");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
}

void 
UHoudiniEditorSubsystem::DumpSessionInfo()
{
    HOUDINI_LOG_MESSAGE(TEXT("network_node_id %i "), object_node_id);
    
    // Get the Display Geo's info
    HAPI_GeoInfo DisplayHapiGeoInfo;
    FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo))
		return;

    FString DisplayName;
    FHoudiniEngineString HoudiniEngineString(DisplayHapiGeoInfo.nameSH);
    HoudiniEngineString.ToFString(DisplayName);
    
    HOUDINI_LOG_MESSAGE(TEXT("DisplayGeo %s NodeID %i  PartCount %i "), *DisplayName, DisplayHapiGeoInfo.nodeId, DisplayHapiGeoInfo.partCount );
}

void 
UHoudiniEditorSubsystem::Fetch()
{
	FetchFromHoudini(NodeSyncOptions.UnrealAssetName, NodeSyncOptions.UnrealPathName);
}

void 
UHoudiniEditorSubsystem::FetchFromHoudini(
	const FString& InPackageName, const FString& InPackageFolder, const int32& InMaxInfluences, const bool& InImportNormals)
{
	// Add a slate notification
	FString Notification = TEXT("Fetching data from Houdini...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Importing To Unreal!"));

 	if (!CreateSessionIfNeeded())
	{
		// For now, just warn the session is not session sync
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: the current session is not session-sync one!"));
	}

	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniEditorSubsystem))
		return;


	FString FetchNodePath = HoudiniEditorSubsystem->NodeSyncOptions.FetchNodePath;

	HAPI_Result result;
	HAPI_NodeId  UnrealContentNode = -1;
	if (!HoudiniEditorSubsystem->NodeSyncOptions.UseOutputNodes)
	{
		// Get the display flag of the geo
		HAPI_GeoInfo DisplayHapiGeoInfo;
		FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
		result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo);
		UnrealContentNode = DisplayHapiGeoInfo.nodeId;
	}
	else
	{
		result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*FetchNodePath), &UnrealContentNode);

		if ((result != HAPI_RESULT_SUCCESS) || (UnrealContentNode < 0))
		{
			//HOUDINI_LOG_MESSAGE(TEXT("Sending Node To %s "), *SendNodePath);
			HOUDINI_LOG_MESSAGE(TEXT("Invalid FetchNodePath"));
			return;
		}
	}
	result = FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), UnrealContentNode, nullptr);

	bool bUseBGEOImport = true;
	bool bReimport = true;
	bool bSuccess = false;
	if (bUseBGEOImport)
	{
		// Create a new Geo importer
		TArray<UHoudiniOutput*> DummyOldOutputs;
		TArray<UHoudiniOutput*> NewOutputs;
		UHoudiniGeoImporter* BGEOImporter = NewObject<UHoudiniGeoImporter>(this);
		BGEOImporter->AddToRoot();

		// Clean up lambda
		auto CleanUp = [&NewOutputs, &BGEOImporter]()
		{
			// Remove the importer and output objects from the root set
			BGEOImporter->RemoveFromRoot();
			for (auto Out : NewOutputs)
				Out->RemoveFromRoot();
		};
		
		// Failure lambda
		auto FailImportAndReturn = [this, &CleanUp, &NewOutputs, &BGEOImporter]()
		{
			CleanUp();

			// Failed to read the file info, fail the import
			//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);

			// TODO: Improve me!
			FString Notification = TEXT("Houdini Node Sync failed!");
			FHoudiniEngineUtils::CreateSlateNotification(Notification);

			return;
		};
		

		// Prepare the package used for creating the mesh, landscape and instancer pacakges
		FHoudiniPackageParams PackageParams;
		PackageParams.PackageMode = EPackageMode::Bake;
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
		PackageParams.HoudiniAssetName = FString();
		PackageParams.BakeFolder = InPackageFolder;//FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
		PackageParams.ObjectName = InPackageName;// FPaths::GetBaseFilename(InParent->GetName());

		if (bReimport)
		{
			PackageParams.ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;
		}
		else
		{
			PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;
		}

		// 4. Get the output from the file node
		if (!BGEOImporter->BuildOutputsForNode(UnrealContentNode, DummyOldOutputs, NewOutputs))
			return FailImportAndReturn();

		// 5. Create the static meshes in the outputs
		const FHoudiniStaticMeshGenerationProperties& StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
		const FMeshBuildSettings& MeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
		if (!BGEOImporter->CreateStaticMeshes(NewOutputs, PackageParams, StaticMeshGenerationProperties, MeshBuildSettings))
			return FailImportAndReturn();

		// 6. Create the curves in the outputs
		if (!BGEOImporter->CreateCurves(NewOutputs, PackageParams))
			return FailImportAndReturn();

		// 7. Create the landscapes in the outputs
		if (!BGEOImporter->CreateLandscapes(NewOutputs, PackageParams))
			return FailImportAndReturn();

		// 8. Create the instancers in the outputs
		if (!BGEOImporter->CreateInstancers(NewOutputs, PackageParams))
			return FailImportAndReturn();

		// Get our result object and "finalize" them
		TArray<UObject*> Results = BGEOImporter->GetOutputObjects();
		for (UObject* Object : Results)
		{
			if (!IsValid(Object))
				continue;

			//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
			Object->MarkPackageDirty();
			Object->PostEditChange();
		}

		CleanUp();

		bSuccess = Results.Num() > 0;
	}
	else
	{
		// TODO:
		// Use the Output translator to support all types of outputs

		// TODO: Add a HoudiniEngineUtils function for this
		// Check more than just one attributes! (only return true if all sk mesh attr are present!)	
		//Determine if Skeletal by presence of capt_xforms detail attribute
		int32 PartId = 0;  //multiple parts broken only do first part to get working
		HAPI_AttributeInfo CaptXFormsInfo;
		FHoudiniApi::AttributeInfo_Init(&CaptXFormsInfo);

		bool bHasSkeletalMeshData = false;
		HAPI_Result CaptXFormsInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			UnrealContentNode, PartId,
			"capt_xforms", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &CaptXFormsInfo);

		bHasSkeletalMeshData = CaptXFormsInfo.exists && (CaptXFormsInfoResult == HAPI_RESULT_SUCCESS);

		if (bHasSkeletalMeshData)
		{
			bSuccess = FetchSkeletalMeshFromHoudini(UnrealContentNode, InPackageName, InPackageFolder, InMaxInfluences, InImportNormals);
		}
		else
		{
			bSuccess = FetchStaticMeshFromHoudini(UnrealContentNode, InPackageName, InPackageFolder);
		}
	}

	if (bSuccess)
	{
		// TODO: Improve me!
		Notification = TEXT("Houdini Node Sync success!");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
	}
	else
	{
		// TODO: Improve me!
		Notification = TEXT("Houdini Node Sync failed!");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
	}
}

bool 
UHoudiniEditorSubsystem::FetchSkeletalMeshFromHoudini(
	const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder, const int32& MaxInfluences, const bool& ImportNormals)
{
    HAPI_NodeInfo MyNodeInfo = FHoudiniApi::NodeInfo_Create();
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &MyNodeInfo))
		return false;

    FString NodeName = FHoudiniEngineUtils::HapiGetString(MyNodeInfo.nameSH);

    // Get the AssetInfo
    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
	// Dont return on failure here...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo))
	{
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to get object info when fetching from Houdini!"));
		//return false;
	}

    FString UniqueName;
    FHoudiniEngineUtils::GetHoudiniAssetName(AssetInfo.nodeId, UniqueName);

	// Get the ObjectInfo
	HAPI_ObjectInfo ObjectInfo;
	FHoudiniApi::ObjectInfo_Init(&ObjectInfo);

	// TODO: Improve on this, parent may not be an OBJ node
	HAPI_NodeId ObjectNodeId = MyNodeInfo.type == HAPI_NodeType::HAPI_NODETYPE_OBJ ? InNodeId : FHoudiniEngineUtils::HapiGetParentNodeId(InNodeId);
	// Dont return on failure here...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjectInfo(FHoudiniEngine::Get().GetSession(), ObjectNodeId, &ObjectInfo))
	{
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to get object info when fetching from Houdini!"));
		//return false;
	}

    FString CurrentAssetName;
    {
		FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
		hapiSTR.ToFString(CurrentAssetName);
    }

    FString CurrentObjectName;
    {
		FHoudiniEngineString hapiSTR(ObjectInfo.nameSH);
		hapiSTR.ToFString(CurrentObjectName);
    }

    //Create a skeletal mesh
    int32 PartId = 0;  //multiple parts broken only do first part to get working
    FString FullPackageName = InPackageFolder + TEXT("/") + InPackageName;
    SKBuildSettings skBuildSettings;
    skBuildSettings.CurrentObjectName = NodeName;
    skBuildSettings.GeoId = InNodeId;
    skBuildSettings.PartId = PartId;
    skBuildSettings.OverwriteSkeleton = NodeSyncOptions.OverwriteSkeleton;
    skBuildSettings.SkeletonAssetPath = NodeSyncOptions.SkeletonAssetPath;

    FHoudiniMeshTranslator::CreateSKAssetAndPackage(skBuildSettings, InNodeId, PartId, FullPackageName, MaxInfluences, ImportNormals);

    TArray<FSkeletalMaterial> Materials;
    FSkeletalMaterial Mat;
    Materials.Add(Mat);
    Materials.Add(Mat);
    FHoudiniMeshTranslator::BuildSKFromImportData(skBuildSettings, Materials);
    //BuildSKFromImportData(SkeletalMeshImportData, Materials, MySkeleton, PackageName, NewMesh, Package);

	return true;
}

bool 
UHoudiniEditorSubsystem::FetchStaticMeshFromHoudini(
	const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder)
{
    // Get the Display Geo's info
    HAPI_GeoInfo DisplayHapiGeoInfo;
    FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
    if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &DisplayHapiGeoInfo))
    {
        HOUDINI_LOG_MESSAGE(TEXT("GetDisplayGeoInfo FAILURE trying to get object_node_id %i "), InNodeId);
        return false;
    }

	// Ensure the node is cooked
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), InNodeId, nullptr))
		return false;

    HAPI_NodeInfo MyNodeInfo = FHoudiniApi::NodeInfo_Create();
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &MyNodeInfo))
		return false;

    FString NodeName = FHoudiniEngineUtils::HapiGetString(MyNodeInfo.nameSH);

    // Get the AssetInfo
    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
	// Dont return on failure here...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo))
	{
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to get object info when fetching from Houdini!"));
		//return false;
	}

    // Get the ObjectInfo
    HAPI_ObjectInfo ObjectInfo;
    FHoudiniApi::ObjectInfo_Init(&ObjectInfo);

	// TODO: Improve on this, parent may not be an OBJ node
	HAPI_NodeId ObjectNodeId = MyNodeInfo.type == HAPI_NodeType::HAPI_NODETYPE_OBJ ? InNodeId : FHoudiniEngineUtils::HapiGetParentNodeId(InNodeId);
	// Dont return on failure here...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjectInfo(FHoudiniEngine::Get().GetSession(), ObjectNodeId, &ObjectInfo))
	{
		HOUDINI_LOG_WARNING(TEXT("HoudiniNodeSync: Failed to get object info when fetching from Houdini!"));
		//return false;
	}		

	// TODO: multiple parts broken only do first part to get working
    //for (int32 PartId = 0; PartId < GeoInfo.partCount; ++PartId)
    int32 PartId = 0;  
    {
		// Get part information.
		HAPI_PartInfo CurrentHapiPartInfo;
		FHoudiniApi::PartInfo_Init(&CurrentHapiPartInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(
			FHoudiniEngine::Get().GetSession(), InNodeId, PartId, &CurrentHapiPartInfo))
			return false;  // continue;

		FString CurrentAssetName;
		{
			FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
			hapiSTR.ToFString(CurrentAssetName);
		}

		FString CurrentObjectName;
		{
			FHoudiniEngineString hapiSTR(ObjectInfo.nameSH);
			hapiSTR.ToFString(CurrentObjectName);
		}

		FString CurrentGeoName;
		{
			FHoudiniEngineString hapiSTR(MyNodeInfo.nameSH);
			hapiSTR.ToFString(CurrentGeoName);
		}

		EHoudiniPartType CurrentPartType = EHoudiniPartType::Invalid;
		switch (CurrentHapiPartInfo.type)
		{
			case HAPI_PARTTYPE_BOX:
			case HAPI_PARTTYPE_SPHERE:
			case HAPI_PARTTYPE_MESH:
			{
				CurrentPartType = EHoudiniPartType::Mesh;
			}
		}
		// Build the HGPO corresponding to this part
		FHoudiniGeoPartObject currentHGPO;
		currentHGPO.AssetId = AssetInfo.nodeId;
		currentHGPO.AssetName = CurrentAssetName;

		currentHGPO.ObjectId = ObjectInfo.nodeId;
		currentHGPO.ObjectName = CurrentObjectName;

		currentHGPO.GeoId = InNodeId;

		currentHGPO.PartId = CurrentHapiPartInfo.id;

		currentHGPO.Type = CurrentPartType;
		currentHGPO.InstancerType = EHoudiniInstancerType::Invalid;

		currentHGPO.TransformMatrix = FTransform::Identity;;

		currentHGPO.NodePath = TEXT("");

		currentHGPO.bIsVisible = ObjectInfo.isVisible && !CurrentHapiPartInfo.isInstanced;
		currentHGPO.bIsEditable = DisplayHapiGeoInfo.isEditable;
		currentHGPO.bIsInstanced = CurrentHapiPartInfo.isInstanced;
		// Never consider a display geo as templated!
		currentHGPO.bIsTemplated = DisplayHapiGeoInfo.isDisplayGeo ? false : DisplayHapiGeoInfo.isTemplated;

		currentHGPO.bHasGeoChanged = DisplayHapiGeoInfo.hasGeoChanged;
		currentHGPO.bHasPartChanged = CurrentHapiPartInfo.hasChanged;
		currentHGPO.bHasMaterialsChanged = DisplayHapiGeoInfo.hasMaterialChanged;
		currentHGPO.bHasTransformChanged = ObjectInfo.hasTransformChanged;

		// Copy the HAPI info caches 
		FHoudiniObjectInfo CurrentObjectInfo;
		FHoudiniOutputTranslator::CacheObjectInfo(ObjectInfo, CurrentObjectInfo);
		currentHGPO.ObjectInfo = CurrentObjectInfo;
		FHoudiniGeoInfo CurrentGeoInfo;
		FHoudiniOutputTranslator::CacheGeoInfo(DisplayHapiGeoInfo, CurrentGeoInfo);
		currentHGPO.GeoInfo = CurrentGeoInfo;
		FHoudiniPartInfo CurrentPartInfo;
		FHoudiniOutputTranslator::CachePartInfo(CurrentHapiPartInfo, CurrentPartInfo);
		currentHGPO.PartInfo = CurrentPartInfo;
		TArray<FHoudiniMeshSocket> PartMeshSockets;
		currentHGPO.AllMeshSockets = PartMeshSockets;

		//Package Parameters
		FHoudiniPackageParams PackageParams;
		PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
		PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

		PackageParams.BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

		//PackageParams.OuterPackage = HAC->GetComponentLevel();
		//PackageParams.HoudiniAssetName = HAC->GetHoudiniAsset() ? HAC->GetHoudiniAsset()->GetName() : FString();
		//PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetName();
		//PackageParams.ComponentGUID = HAC->GetComponentGUID();
		PackageParams.ObjectName = FString();
		PackageParams.NameOverride = InPackageName;
		PackageParams.FolderOverride = InPackageFolder;
		PackageParams.OverideEnabled = true;

		FHoudiniMeshTranslator CurrentTranslator;
		CurrentTranslator.ForceRebuild = true;
		CurrentTranslator.SetHoudiniGeoPartObject(currentHGPO);
		//CurrentTranslator.SetInputObjects(InOutputObjects);
		//CurrentTranslator.SetOutputObjects(OutOutputObjects);
		//CurrentTranslator.SetInputAssignmentMaterials(AssignmentMaterialMap);
		//CurrentTranslator.SetAllOutputMaterials(InAllOutputMaterials);
		//CurrentTranslator.SetReplacementMaterials(ReplacementMaterialMap);
		CurrentTranslator.SetPackageParams(PackageParams, true);
		//CurrentTranslator.SetTreatExistingMaterialsAsUpToDate(bInTreatExistingMaterialsAsUpToDate);
		//CurrentTranslator.SetStaticMeshGenerationProperties(InSMGenerationProperties);
		//CurrentTranslator.SetStaticMeshBuildSettings(InSMBuildSettings);

		// TODO: Fetch from settings/HAC
		CurrentTranslator.DefaultMeshSmoothing = 1;

		CurrentTranslator.CreateStaticMesh_RawMesh();	
    }

	return true;
}

/*
void UHoudiniEditorSubsystem::RegisterLayoutExtensions(FLayoutExtender& Extender)
{
	Extender.ExtendLayout(LevelEditorTabIds::PlacementBrowser, ELayoutExtensionPosition::Before, FTabManager::FTab(UAssetEditorUISubsystem::TopLeftTabID, ETabState::ClosedTab));
}
*/