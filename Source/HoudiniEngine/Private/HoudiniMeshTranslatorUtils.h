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

#pragma once

#include "CoreMinimal.h"
#include "HAPI/HAPI_Common.h"
#include "HoudiniOutput.h"
#include "Engine/StaticMesh.h"

#include "HoudiniMeshTranslatorUtils.generated.h"

class UMaterialInterface;
class UMeshComponent;
class USceneComponent;
class UHoudiniOutput;
class UStaticMesh;

class FEditPropertyChain;
struct FHoudiniCachedMeshPart;
struct FHoudiniGeoPartObject;
struct FHoudiniMeshTranslationContext;
struct FHoudiniPackageParams;
struct FHoudiniSplitGroupMesh;
class FProperty;

UENUM()
enum class EHoudiniSplitType : uint8
{
	Invalid,

	Normal,

	LOD,

	RenderedComplexCollider,
	InvisibleComplexCollider,

	RenderedUCXCollider,
	InvisibleUCXCollider,

	RenderedSimpleCollider,
	InvisibleSimpleCollider
};

class HOUDINIENGINE_API FHoudiniMeshTranslatorUtils
{
public:
	template <typename TYPE>
	static int32 TransferPartAttributesToSplit(
		const TArray<int32>& InVertexList,
		const HAPI_AttributeInfo& InAttribInfo,
		const TArray<TYPE>& InData,
		TArray<TYPE>& OutSplitData);

	static int32 TransferRegularPointAttributesToVertices(const TArray<int32>& InVertexList, const HAPI_AttributeInfo& InAttribInfo,
		const TArray<float>& InData, TArray<float>& OutVertexData);

	static int32 GenerateBoxAsSimpleCollision(const TArray<FVector>& Positions, FKAggregateGeom& Collisions);

	static int32 GenerateOrientedBoxAsSimpleCollision(const TArray<FVector>& Positions, FKAggregateGeom& Collisions);

	static int32 GenerateSphereAsSimpleCollision(const TArray<FVector>& Positions, FKAggregateGeom& Collisions);

	static int32 GenerateSphylAsSimpleCollision(const TArray<FVector>& Positions, FKAggregateGeom& Collisions);

	static int32 GenerateOrientedSphylAsSimpleCollision(const TArray<FVector>& Positions, FKAggregateGeom& Collisions);

	static int32 GenerateKDopAsSimpleCollision(const TArray<FVector>& Positions, const TArray<FVector>& Directions, FKAggregateGeom& Collisions);

	static bool AddSimpleCollisionToAggregate(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& InSplitGroupName,
		FKAggregateGeom& OutAggregateCollisions);

	static FKAggregateGeom BuildAggregateCollision(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FHoudiniSplitGroupMesh& InMesh);

	static TArray<FVector> GetKdopDirections(const FString& SplitGroupName);

	static void CalcBoundingBox(const TArray<FVector>& Positions, FVector& Center, FVector& Extents, FVector& LimitVector);

	static void CalcBoundingSphere(const TArray<FVector>& Positions, FSphere& Sphere, FVector& LimitVector);

	static void CalcBoundingSphere2(const TArray<FVector>& Positions, FSphere& Sphere, FVector& LimitVector);

	static void CalcBoundingSphyl(const TArray<FVector>& Positions, FSphere& Sphere, float& Length, FRotator& Rotation, FVector& LimitVector);

	static bool IsMeshBuildTimingEnabled();

	static EHoudiniSplitType GetSplitTypeFromSplitName(const FString& InSplitName);

	static FString GetMeshIdentifierFromSplit(const FString& InSplitName, EHoudiniSplitType InSplitType);

	static bool ExtractMaterialIndex(FString& MaterialName, int32& MatIndex);

	static void UpdateMeshBuildSettings(const FMeshBuildSettings& InStaticMeshBuildSettings, FMeshBuildSettings& OutMeshBuildSettings,
		bool bHasNormals, bool bHasTangents, bool bHasLightmapUVSet);

	static void UpdateStaticMeshNaniteSettings(int32 GeoId, int32 PartId, int32 PrimIndex, UStaticMesh* StaticMesh);

	static void CopyAttributesFromHGPOForSplit(const FHoudiniGeoPartObject& InHGPO, int32 InPointIndex, int32 InPrimIndex,
		TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens);

	static void CopyAttributesFromHGPOForSplit(const FHoudiniGeoPartObject& InHGPO, const FHoudiniCachedMeshPart& InCachedMeshPart,
		const FString& InSplitGroupName, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens);

	static void CopyAttributesFromHGPOForSplit(const FHoudiniGeoPartObject& InHGPO, const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens);

	static FHoudiniOutputObjectIdentifier MakeOutputObjectIdentifier(const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniCachedMeshPart& InCachedMeshPart, const FString& InSplitGroupName, EHoudiniSplitType InSplitType);

	static bool HavePartMaterialsChanged(const FHoudiniGeoPartObject& InHGPO, FHoudiniCachedMeshPart& InOutCachedMeshPart);

	static int32 GetFaceMaterialIndex(UMaterialInterface* MaterialInterface,
		TMap<TObjectPtr<UMaterialInterface>, int32>& MaterialToIndex, int32 MatIndex, TArray<FStaticMaterial>& StaticMaterials);

	static float GetLODSCreensizeForSplit(const FHoudiniGeoPartObject& InHGPO, FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& SplitGroupName);

	static bool IsGammaCorrectionDisabled(HAPI_NodeId NodeId, HAPI_PartId PartId);

	static bool TryToFindPropertyOnSourceModel(UStaticMesh* InStaticMesh, int32 InSourceModelIndex, const FString& InPropertyName,
		FEditPropertyChain& InPropertyChain, bool& bOutSkipDefaultIfPropertyNotFound, FProperty*& OutFoundProperty,
		UObject*& OutFoundPropertyObject, void*& OutContainer);

	static bool TryToFindPropertyOnSourceModel(FStaticMeshSourceModel& InSourceModel, const FString& InPropertyName,
		FEditPropertyChain& InPropertyChain, FProperty*& OutFoundProperty, void*& OutContainer);

	static void GetCustomPrimitiveData(
		int32 InGeoNodeId,
		int32 InPartId,
		int32 InPrimIndex,
		TArray<float>& OutCustomPrimData,
		HAPI_AttributeOwner InAttribOwner = HAPI_AttributeOwner::HAPI_ATTROWNER_INVALID);

	static void SetCustomPrimitiveData(
		const TArray<float>& CustomPrimData,
		USceneComponent* InComponentToUpdate);

	static bool CreateNeededMaterials(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		FHoudiniPackageParams& InOutPackageParams,
		UObject* InOuterComponent,
		const FHoudiniMeshTranslationContext& InTranslationContext,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InInputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials);

	static UMeshComponent* CreateMeshComponent(UObject* InOuterComponent, const TSubclassOf<UMeshComponent>& InComponentType);

	static void UpdateMeshComponent(
		UMeshComponent* InMeshComponent,
		UObject* InMesh,
		const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
		const FHoudiniOutputObject& OutputObject,
		const FHoudiniGeoPartObject* InHGPO,
		TArray<TObjectPtr<AActor>>& HoudiniCreatedSocketActors,
		TArray<TObjectPtr<AActor>>& HoudiniAttachedSocketActors,
		bool bInApplyGenericProperties = true);

	static UMeshComponent* CreateOrUpdateMeshComponent(
		const UHoudiniOutput* InOutput,
		UObject* InOuterComponent,
		const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
		const TSubclassOf<UMeshComponent>& InComponentType,
		FHoudiniOutputObject& OutOutputObject,
		const FHoudiniGeoPartObject*& OutFoundHGPO,
		bool& bCreated);

	static void UpdateSplitGroups(FHoudiniGeoPartObject& InOutHGPO);
};
 
