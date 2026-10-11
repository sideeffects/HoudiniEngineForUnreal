/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*/

#pragma once

#include "HoudiniMeshTranslator.h"

class UDynamicMesh;

class HOUDINIENGINE_API FHoudiniDynamicMeshTranslator
{
public:
	// A detail integer attribute that opts this mesh part into the Dynamic Mesh output path.
	static bool IsDynamicMeshOutput(const FHoudiniGeoPartObject& InHGPO);

	static bool CreateDynamicMeshesFromHoudiniGeoPartObject(
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutAssignmentMaterialMap,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterialMap,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
		UObject* InOuterComponent,
		bool bInForceRebuild,
		bool bInTreatExistingMaterialsAsUpToDate);

private:
	static bool CreateDynamicMeshesFromSplitGroups(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
		UObject* InOuterComponent,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials);

	static bool CreateDynamicMeshFromSplitGroup(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		FHoudiniSplitGroupMesh& InOutSplitMeshData,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
		UObject* InOuterComponent,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials);

	static bool BuildDynamicMesh(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& InSplitGroupName,
		const TArray<int32>& InFaceMaterialIndices,
		UDynamicMesh& OutDynamicMesh);

	static void BuildMaterialSlots(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& InSplitGroupName,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		TArray<TObjectPtr<UMaterialInterface>>& OutMaterials,
		TArray<int32>& OutFaceMaterialIndices);
};
