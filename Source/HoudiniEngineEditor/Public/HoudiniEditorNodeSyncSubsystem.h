// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniInput.h"

#include "HoudiniEditorNodeSyncSubsystem.generated.h"


class USkeletalMesh;

UENUM()
enum class EHoudiniNodeSyncStatus : uint8
{
	// Fetch/Send not used yet
	None,
	
	// Last operation failed
	Failed,
	
	// Last operation was successfull
	Success,
	
	// Last operation was succesfull, but reported errors
	SuccessWithErrors,

	// Sending/Fetching
	Running,

	// Display a warning
	Warning
};

USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniNodeSyncOptions
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString FetchNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString SendNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString UnrealAssetName = "TestAsset";

	UPROPERTY()
	FString UnrealAssetFolder = "/Game/000";

	UPROPERTY()
	bool bUseOutputNodes = true;

	UPROPERTY()
	bool bFetchToWorld = false;

	UPROPERTY()
	FString UnrealActorName = "HoudiniActor";

	UPROPERTY()
	FString UnrealActorFolder = "/Houdini/NodeSync";

	UPROPERTY()
	bool bReplaceExisting = false;

	UPROPERTY()
	bool bOverwriteSkeleton = false;

	UPROPERTY()
	FString SkeletonAssetPath = "";	
};


/**
 * Editor Susbsystem that creates a "Managed" Session HDA used to transfer assets between Houdini and Unreal
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorNodeSyncSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendToHoudini(const TArray<UObject*>& SelectedAssets);

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void FetchFromHoudini(const FString& InPackageName, const FString& InPackageFolder, const int32& MaxInfluences = 1, const bool& ImportNormals=false);
	
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void Fetch();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendWorldSelection();

	bool CreateSessionIfNeeded();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void DumpSessionInfo();

	// Returns the color corresponding to a given node sync status
	static FLinearColor GetStatusColor(const EHoudiniNodeSyncStatus& Status);

	// Node sync options
	FHoudiniNodeSyncOptions NodeSyncOptions;

	// SEND status
	EHoudiniNodeSyncStatus LastSendStatus = EHoudiniNodeSyncStatus::None;
	FString SendStatusMessage;
	FString SendStatusDetails;

	// FETCH status
	EHoudiniNodeSyncStatus LastFetchStatus = EHoudiniNodeSyncStatus::None;
	FString FetchStatusMessage;
	FString FetchStatusDetails;

	bool GetNodeSyncInput(UHoudiniInput*& OutInput);

	//virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

	bool GatherAllFetchNodeIds(
		HAPI_NodeId UnrealFetchNodeId,
		const bool bUseOutputNodes,
		TArray<HAPI_NodeId>& OutOutputNodes);

private:

	bool InitNodeSyncInputIfNeeded();

	bool SendStaticMeshToHoudini(const HAPI_NodeId& InMeshNodeId, UStaticMesh* InMesh);
	bool SendSkeletalMeshToHoudini(const HAPI_NodeId& InMeshNodeId, USkeletalMesh* InSkelMesh);
	bool FetchSkeletalMeshFromHoudini(const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder, const int32& MaxInfluences, const bool& ImportNormals);
	bool FetchStaticMeshFromHoudini(const HAPI_NodeId& InNodeId, const FString& InPackageName, const FString& InPackageFolder);

	HAPI_NodeId object_node_id = -1;
	//HAPI_NodeId network_node_id = -1;

	//UPROPERTY()
	UHoudiniInput* NodeSyncInput;
};
