// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEditorSubsystem.generated.h"


class USkeletalMesh;

USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniNodeSync
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
	FString UnrealPathName = "/Game/000";

	UPROPERTY()
	bool UseDisplayFlag = false;

};


/**
 * Editor Susbsystem that creates a "Managed" Session HDA used to transfer assets between Houdini and Unreal
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendToHoudini(const TArray<FAssetData>& SelectedAssets);

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendToUnreal(FString PackageName, FString PackageFolder, int MaxInfluences = 1, bool ImportNormals=false);
	
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void Fetch();

	void CreateSessionHDA();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void DumpSessionInfo();

	FHoudiniNodeSync NodeSync;

private:
	void SendStaticMeshToHoudini(HAPI_NodeId mesh_node_id, UStaticMesh* SkelMesh);
	void SendSkeletalMeshToHoudini(HAPI_NodeId mesh_node_id, USkeletalMesh* SkelMesh);
	void SendSkeletalMeshToUnreal(HAPI_NodeId NodeId, FString PackageName, FString PackageFolder, int MaxInfluences, bool ImportNormals);
	void SendStaticMeshToUnreal(HAPI_NodeId NodeId, FString PackageName, FString PackageFolder);



	HAPI_NodeId object_node_id = -1;
	//HAPI_NodeId network_node_id = -1;
};
