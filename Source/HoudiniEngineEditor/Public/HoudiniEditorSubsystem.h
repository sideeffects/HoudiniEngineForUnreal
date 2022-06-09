// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEditorSubsystem.generated.h"


class USkeletalMesh;
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
	void SendToUnreal(FString PackageName, FString PackageFolder, int MaxInfluences, bool ImportNormals);
	
	void CreateSessionHDA();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void DumpSessionInfo();

private:
	void SendStaticMeshToHoudini(UStaticMesh* SkelMesh);
	void SendSkeletalMeshToHoudini(USkeletalMesh* SkelMesh);
	void SendSkeletalMeshToUnreal(FString PackageName, FString PackageFolder, int MaxInfluences, bool ImportNormals);
	void SendStaticMeshToUnreal(FString PackageName, FString PackageFolder);



	HAPI_NodeId object_node_id = -1;
	//HAPI_NodeId network_node_id = -1;
};
