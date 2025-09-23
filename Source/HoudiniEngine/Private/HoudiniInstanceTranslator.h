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

#include "HAPI/HAPI_Common.h"
#include "HoudiniOutput.h"
#include "CoreMinimal.h"
#include "HoudiniEngineString.h"
#include "UObject/ObjectMacros.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniMaterialTranslator.h"
#include "HoudiniInstanceTranslator.generated.h"

struct FHoudiniInstancer;
class UStaticMesh;
class UFoliageType;
class UHoudiniStaticMesh;
class UHoudiniInstancedActorComponent;
struct FHoudiniPackageParams;

enum InstancerComponentType
{
	Invalid = -1,
	InstancedStaticMeshComponent = 0,
	HierarchicalInstancedStaticMeshComponent = 1,
	//MeshSplitInstancerComponent = 2,
	HoudiniInstancedActorComponent = 3,
	StaticMeshComponent = 4,
	HoudiniStaticMeshComponent = 5,
	Foliage = 6,
	GeometryCollectionComponent = 7,
	LevelInstance = 8
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniInstancerSettings
{
public:

	GENERATED_BODY()

	UPROPERTY()
	FString LevelPath;

	UPROPERTY()
	FString BakeActorName;

	UPROPERTY()
	FString BakeActorClassName;

	UPROPERTY()
	FString BakeOutlinerFolder;

	UPROPERTY()
	FString BakeFolder;

	UPROPERTY()
	FString OutputName;

	UPROPERTY()
	bool bIsFoliage = false;

	UPROPERTY()
	bool bForceHISM = false;

	UPROPERTY()
	bool bForceInstancer = false;

	// Transform relative to the parent (HAC) Transform.
	UPROPERTY()
	FTransform ComponentRelativeTransform = FTransform::Identity;

	// Data Layers which should be applied (during Baking only).
	UPROPERTY()
	TArray<FHoudiniDataLayer> DataLayers;

	// HLOD Layers which should be applied (during Baking only).
	UPROPERTY()
	TArray<FHoudiniHLODLayer> HLODLayers;

};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniInstancer
{
	// This structure holds data about a specific instancer. AttributeIndices references
	// data in the FHoudiniInstancerPartData which is a part of (eg. transforms).

	GENERATED_BODY()

	UPROPERTY()
	FString ObjectPath;

	UPROPERTY()
	TArray<int> AttributeIndices;

	UPROPERTY()
	FString SplitName;

	UPROPERTY()
	FHoudiniInstancerSettings Settings;

	UPROPERTY()
	int NumCustomFloats = 0;

	UPROPERTY()
	TArray<float> CustomFloats;

	UPROPERTY()
	bool bVisible = true;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniInstancerPartData
{
public:
	
	GENERATED_BODY()

	UPROPERTY()
	FHoudiniGeoPartObject GeoPartObject;

	UPROPERTY()
	TArray<FHoudiniInstancer> Instancers;

	UPROPERTY()
	TArray<FTransform>  InstanceTransforms;

	UPROPERTY()
	FString SplitAttributeName;

	UPROPERTY()
	TArray<FHoudiniGenericAttribute> AllPropertyAttributes;

	// Array of material attributes
	// If multiple slots are defined, we store all the different attributes values in a flat array
	// Such that the size of MaterialAttributes is NumberOfAttributes * NumberOfMaterialSlots
	UPROPERTY()
	TArray<FHoudiniMaterialInfo> MaterialAttributes;
};

struct HOUDINIENGINE_API FHoudiniInstanceTranslator
{
	public:

	static bool IsHISM(HAPI_NodeId GeoId, HAPI_NodeId PartId, HAPI_AttributeOwner Owner, int Index);
	static FHoudiniInstancerPartData PopulateInstancedOutputPartData(
		const FHoudiniGeoPartObject& InHGPO,
		const TArray<UHoudiniOutput*>& InAllOutputs);

	static int CreateAllInstancersFromHoudiniOutputs(
		const TArray<UHoudiniOutput*>& InAllOutputs,
		UObject* InOuterComponent,
		const FHoudiniPackageParams& InPackageParms,
		const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData = nullptr);

	static int CreateAllInstancersFromHoudiniOutputs(
		const TArray<UHoudiniOutput*>& OutputsToUpdate,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		UObject* InOuterComponent,
		const FHoudiniPackageParams& InPackageParms,
		const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData = nullptr);

	static bool CreateAllInstancersFromHoudiniOutput(
		UHoudiniOutput* InOutput,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		UObject* InOuterComponent,
		const FHoudiniPackageParams& InPackageParams,
		const TMap<FHoudiniOutputObjectIdentifier,FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData = nullptr);

	static bool GetAttributeInstancerPartData(const FHoudiniGeoPartObject& HGPO, FHoudiniInstancerPartData& PartData);

	static bool GetPackedPrimitiveInstancerPartData(
		const FHoudiniGeoPartObject& HGPO, 
		const TArray<UHoudiniOutput*> & InAllOutputs, 
		FHoudiniInstancerPartData& PartData);

	static bool FindInstancedOutputObject(
		const FHoudiniGeoPartObject& HGPO,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniOutputObjectIdentifier& OutId,
		const UHoudiniOutput*& OutOutput);

	static void SetInstancerObject(
		FHoudiniInstancer& InstancerData,
		const FHoudiniGeoPartObject& HGPO,
		const TArray<UHoudiniOutput*>& InAllOutputs);

	// Creates a new component or updates the previous one if possible
	static bool CreateInstancer(
		const FHoudiniOutputObjectIdentifier& Id,
		FHoudiniOutputObject & Output,
		UObject*& InstanceObject,
		const FHoudiniInstancer& Instancer,
		const FHoudiniInstancerPartData& InstancerPartData,
		const FHoudiniPackageParams& InPackageParams,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface *>& InstancerMaterials);

	// Create or update an ISMC / HISMC
	static bool CreateInstancedStaticMeshInstancer(
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancerPartData,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface*>& InstancerMaterials);

	// Create or update an IAC
	static bool CreateInstancedActorInstancer(
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancePartData,
		USceneComponent* ParentComponent);

	// Create or update a StaticMeshComponent (when we have only one instance)
	static bool CreateStaticMeshInstancer(
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancerPartData,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface*>& InstancerMaterials);

	// Create or update a HoudiniStaticMeshComponent (when we have only one instance)
	static bool CreateHoudiniStaticMeshInstancer(
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancerPartData,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface*>& InstancerMaterials);

	// Create or update a Foliage instances
	static bool CreateFoliageInstancer(
		const FHoudiniOutputObjectIdentifier& Id,
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancerPartData,
		const FHoudiniPackageParams& InPackageParams,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface*>& InstancerMaterials);

	// Create or update Level instances
	static bool CreateLevelInstanceInstancer(
		FHoudiniOutputObject& Output,
		const FHoudiniInstancer& Instancer,
		UObject* InstanceObject,
		const FHoudiniInstancerPartData& InstancerPartData,
		USceneComponent* ParentComponent,
		const TArray<UMaterialInterface*>& InstancerMaterials);

	// Utility function
	// Fetches instance transforms and convert them to unreal coordinates
	static bool HapiGetInstanceTransforms(
		const FHoudiniGeoPartObject& InHGPO,
		TArray<FTransform>& OutInstancerUnrealTransforms);

	// Helper function used to spawn a new Actor for UHoudiniInstancedActorComponent
	// Relies on editor-only functionalities, so this function is not on the IAC itself
	static AActor* SpawnInstanceActor(
		const FTransform& InTransform,
		ULevel* InSpawnLevel, 
		UHoudiniInstancedActorComponent* InIAC,
		AActor* InReferenceActor,
		FName Name = NAME_None);

	// Helper functions for generic property attributes
	static bool GetGenericPropertiesAttributes(
		int32 InGeoNodeId,
		int32 InPartId,
		TArray<FHoudiniGenericAttribute>& OutPropertyAttributes);

	static bool GetMaterialOverridesFromAttributes(
		int32 InGeoNodeId,
		int32 InPartId,
		int32 InAttributeIndex,
		const FString& InAttributeName,
		const TArray<FString>& InAllAttribNames,
		TArray<FString>& OutMaterialAttributes);

	static bool GetMaterialOverridesFromAttributes(
		int32 InGeoNodeId,
		int32 InPartId, 
		int32 InAttributeIndex,
		EHoudiniInstancerType InInstancerType,
		TArray<FHoudiniMaterialInfo>& OutMaterialAttributes);

	static bool GetInstancerMaterials(
		const TArray<FHoudiniMaterialInfo>& MaterialAttribute,
		TArray<UMaterialInterface*>& OutInstancerMaterials);

	static bool GetInstancerMaterialInstances(
		const TArray<FHoudiniMaterialInfo>& MaterialAttribute,
		const FHoudiniGeoPartObject& InHGPO, const FHoudiniPackageParams& InPackageParams,
		TArray<UMaterialInterface*>& OutInstancerMaterials);

	static TArray<UMaterialInterface*> GetAllInstancerMaterials(
		int32 InAttributeIndex,
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams);

	static FString GetInstancerTypeFromComponent(UObject* InComponent);

	static bool IsForceInstancer(HAPI_NodeId GeoId, HAPI_NodeId PartId, HAPI_AttributeOwner Owner, int Index);

	// Checks for PerInstanceCustomData on the instancer part
	static void GetPerInstanceCustomData(
		int32 InGeoNodeId,
		int32 InPartId,
		FHoudiniInstancerPartData& OutInstancedOutputPartData);

	// Update PerInstanceCustom data on the given component if possible
	static void SetPerInstanceCustomData(
		const FHoudiniInstancer& Instancers, 
		const FHoudiniInstancerPartData& InstancePartData,
		USceneComponent* InComponentToUpdate);

	static void SetGenericPropertyAttributes(UObject* Object, 
		const FHoudiniInstancer& InstancerData, 
		const FHoudiniInstancerPartData& InInstancedOutputPartData);

	static FHoudiniInstancerSettings GetDefaultInstancerSettings(const FHoudiniGeoPartObject& HGPO, HAPI_AttributeOwner Owner);

	static FHoudiniInstancerSettings GetInstancerSettings(const FHoudiniGeoPartObject& HGPO, HAPI_AttributeOwner AttributeOwner, int Index, const FHoudiniInstancerSettings & Defaults);

	static TArray<FTransform> UnpackTransforms(const FHoudiniInstancer& InstanceData, const FHoudiniInstancerPartData & PartData);

	static TTuple<FString, FHoudiniEngineIndexedStringMap> GetSplitData(const FHoudiniGeoPartObject& HGPO, HAPI_AttributeOwner Owner);

	static TArray<FTransform> GetInstancerTransforms(const FHoudiniGeoPartObject& InHGPO);

	static UObject* LoadInstancedObject(const FString& ObjectPath);

};